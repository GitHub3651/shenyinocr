#pragma once

#include "detection/common/detection_pose.h"
#include "detection/common/frame_preprocessor.h"
#include "detection/multi_template_runtime_snapshot.h"
#include "templates/template_store.h"
#include "runtime/detection_worker.h"
#include "runtime/inspection_plc_controller.h"
#include "detection/detection_registry.h"
#include "runtime/result_presentation_mailbox.h"
#include "runtime/result_service.h"
#include "system_support/settings/app_settings.h"

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

enum class InspectionRuntimeState
{
    Idle,
    Starting,
    Running,
    Stopping,
    Fault
};

enum class InspectionFaultReason
{
    None,
    CameraDisconnected,
    PlcDisconnected,
    HardTriggerQueueOverflow,
    RuntimeInvariantViolation,
    BarcodeCsvUnavailable
};

struct InspectionFaultSnapshot
{
    InspectionFaultReason reason = InspectionFaultReason::None;
    quint64 acceptedProductCount = 0;
    quint64 finalizedProductCount = 0;
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
        const MultiTemplateRuntimeSnapshot &multiTemplateSnapshot,
        double tissueRoughnessThreshold,
        const FramePreprocessSettings &framePreprocess);
    bool commitStart();
    void rollbackStart();
    bool beginStop();
    void waitForStop();
    void finishStop(bool writeRunSummary = true);

    bool enterFault(
        InspectionFaultReason reason,
        const QString &diagnostic = QString());

    InspectionRuntimeState state() const;
    bool isBusy() const;
    bool isRunning() const;
    bool isPlcConnected() const;
    bool requiresPlcForRun() const;
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

    QImage renderPreviewFrame(const cv::Mat &image);

    DetectionRuntimeReadiness prepareDetection(DetectionMode mode) const;
    bool startDetection(
        const ResultServiceRunConfiguration &resultConfiguration,
        QString *errorMessage);
    std::shared_ptr<const FrameData> acceptFrame(
        const cv::Mat &image,
        quint64 frameNumber = 0,
        int cameraIndex = 0);
    DetectionCompletion complete(
        const std::shared_ptr<const FrameData> &frame,
        const DetectionResult &result);
    bool finalizeResultClaim(const ProductKey &productKey);

    bool isDetectionWorkerActive() const;
    std::size_t detectionWorkerQueueCapacity() const;
    bool submitDetectionFrame(
        const std::shared_ptr<const FrameData> &frame);
    DetectionWorkSubmissionResult trySubmitDetectionFrame(
        const std::shared_ptr<const FrameData> &frame);

    DetectionResultStatistics statistics() const;
    void resetStatistics();
    void resetNgCount();
    int clearPendingDelayedNgRequests();

signals:
    void presentationReady(InspectionPresentation presentation);
    void faultSnapshotChanged(InspectionFaultSnapshot snapshot);

private:
    friend class ResultService;

    bool publishPresentation(
        const InspectionPresentation &presentation);

private slots:
    void drainPresentationMailbox();

private:
    enum class ProductProgress
    {
        Accepted,
        Claimed
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
    quint64 m_finalizedProductCount = 0;
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
    std::atomic<bool> m_wakePosted{false};
    std::atomic<bool> m_detectionWorkerActive{false};
};
