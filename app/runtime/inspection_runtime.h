// 文件作用：本文件用于管理正式检测生命周期、线程、队列、故障状态和结果链的唯一运行实例。
// 主要职责：管理正式检测生命周期、线程、队列、故障状态和结果链的唯一运行实例。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/positioning/detection_pose.h"
#include "detection/common/frame_preprocessor.h"
#include "detection/detection_template_snapshot.h"
#include "templates/template_store.h"
#include "runtime/detection_worker.h"
#include "runtime/inspection_plc_controller.h"
#include "detection/detection_registry.h"
#include "runtime/result_presentation_mailbox.h"
#include "runtime/result_service.h"
#include "system_support/settings/app_settings.h"

#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

struct InspectionRunContext;

// 组件说明：InspectionRuntimeState 枚举列出该组件允许使用的稳定状态和选项。
enum class InspectionRuntimeState
{
    Idle,
    Starting,
    Running,
    Stopping,
    Fault
};

// 组件说明：InspectionFaultReason 枚举列出该组件允许使用的稳定状态和选项。
enum class InspectionFaultReason
{
    None,
    CameraDisconnected,
    PlcDisconnected,
    HardTriggerQueueOverflow,
    ProductIdentityAmbiguous,
    RuntimeInvariantViolation
};

Q_DECLARE_METATYPE(InspectionFaultReason)

// 组件说明：InspectionFaultSnapshot 数据结构集中传递该流程需要的只读数据或回调。
struct InspectionFaultSnapshot
{
    InspectionFaultReason reason = InspectionFaultReason::None;
    QString diagnostic;
    QString runId;
    quint64 acceptedProductCount = 0;
    quint64 completedProductCount = 0;
    quint64 postFaultDroppedFrameCount = 0;
    QDateTime occurredAtUtc;

    // 函数说明：isActive 函数检查相关状态并返回判断结果。
    bool isActive() const
    {
        return reason != InspectionFaultReason::None;
    }
};

Q_DECLARE_METATYPE(InspectionFaultSnapshot)

// Owns one complete production run. Frame admission never creates a missing
// run implicitly; beginStart() is the only run creation point.
class InspectionRuntime : public QObject
{
    Q_OBJECT

public:
    using RunIdFactory = std::function<QString()>;

    explicit InspectionRuntime(
        const RunIdFactory &runIdFactory,
        const std::shared_ptr<InspectionPlcController> &plcController,
        const std::shared_ptr<DetectionRegistry> &detectionRegistry);
    ~InspectionRuntime() override;

    QString beginStart(
        const AppSettings &machineSettings,
        DetectionMode mode,
        const QVector<PreparedTemplateSnapshot> &preparedTemplates,
        const DetectionTemplateSnapshot &templateSnapshot,
        double tissueRoughnessThreshold,
        const FramePreprocessSettings &framePreprocess);
    bool commitStart();
    void rollbackStart();
    bool beginStop();
    void waitForStop();
    void finishStop();

    bool enterFault(
        InspectionFaultReason reason,
        const QString &diagnostic = QString(),
        const QDateTime &occurredAtUtc = QDateTime());
    int reconcileFaultProducts();
    bool acknowledgeFault();

    InspectionRuntimeState state() const;
    InspectionFaultSnapshot faultSnapshot() const;
    bool isBusy() const;
    bool isRunning() const;
    QString runId() const;
    quint64 acceptedProductCount() const;
    quint64 completedProductCount() const;

    bool isPlcConnected() const;
    PlcOperationResult connectPlc(
        const QString &address,
        int rack,
        int slot);
    PlcOperationResult disconnectPlc();
    PlcOperationResult writePlcTriggerMode(int modeIndex);
    InspectionPlcRunSettingsResult applyPlcRunSettings(
        const InspectionPlcRunSettings &settings);
    PlcOperationResult writePlcPhotoDistance(std::uint32_t photoDistance);
    PlcOperationResult writePlcResultValue(std::uint8_t value);

    DetectionRuntimeReadiness prepareDetection(DetectionMode mode) const;
    bool startDetection(
        const ResultServiceRunConfiguration &resultConfiguration,
        QString *errorMessage);
    std::shared_ptr<const FrameData> acceptFrame(
        const cv::Mat &image,
        quint64 frameNumber = 0,
        int cameraIndex = 0,
        const QDateTime &timestampUtc = QDateTime());
    DetectionCompletion complete(
        const std::shared_ptr<const FrameData> &frame,
        const DetectionResult &result);
    bool claimResult(const ProductKey &productKey);

    bool isDetectionWorkerActive() const;
    std::size_t detectionWorkerQueueCapacity() const;
    bool submitDetectionFrame(
        const std::shared_ptr<const FrameData> &frame);
    DetectionWorkSubmissionResult trySubmitDetectionFrame(
        const std::shared_ptr<const FrameData> &frame);

    bool submitUiCompletion(const UiCompletionMailbox::Work &work);
    bool processOneUiCompletion();
    void cancelUiCompletion();

    ResultService &resultService();
    const ResultService &resultService() const;
    DetectionResultStatistics statistics() const;
    DetectionAbnormalStatistics abnormalStatistics() const;
    int totalCount() const;
    int ngCount() const;
    int pendingDelayedNgCount() const;
    void resetStatistics();
    void resetAbnormalStatistics();
    void resetNgCount();
    void clearPendingDelayedNgRequests();

signals:
    void presentationReady(InspectionPresentation presentation);
    void imageSaveFailed(quint64 totalFailed, QString latestError);
    void roiWarningChanged(bool active);
    void faultSnapshotChanged(InspectionFaultSnapshot snapshot);

private:
    friend class ResultService;

    bool publishPresentation(
        const InspectionPresentation &presentation);
    void publishImageSaveFailure(
        quint64 totalFailed,
        const QString &latestError);
    void publishRoiWarning(bool active);

    // 组件说明：ProductProgress 枚举列出该组件允许使用的稳定状态和选项。
    enum class ProductProgress
    {
        Accepted,
        AlgorithmCompleted
    };

    QString createRunId() const;
    void requestDetectionWorkerStop();
    void waitForDetectionWorkerStop();
    bool belongsToCurrentRun(const ProductKey &productKey) const;

    RunIdFactory m_runIdFactory;
    mutable std::mutex m_mutex;
    InspectionRuntimeState m_state = InspectionRuntimeState::Idle;
    std::shared_ptr<const InspectionRunContext> m_runContext;
    quint64 m_acceptedProductSequence = 0;
    quint64 m_completedProductCount = 0;
    quint64 m_lastCompletedProductSequence = 0;
    InspectionFaultSnapshot m_faultSnapshot;
    std::map<quint64, std::weak_ptr<const FrameData>> m_acceptedFrames;
    std::map<quint64, ProductProgress> m_products;

    std::shared_ptr<InspectionPlcController> m_plcController;
    std::shared_ptr<DetectionRegistry> m_detectionRegistry;
    std::unique_ptr<ResultService> m_resultService;

    mutable std::mutex m_detectionWorkerMutex;
    std::shared_ptr<DetectionWorker> m_detectionWorker;
    UiCompletionMailbox m_uiCompletionMailbox;
    std::atomic<bool> m_detectionWorkerActive{false};
};
