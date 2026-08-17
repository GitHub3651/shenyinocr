#pragma once

#include "TrackingTypes.h"
#include "runtime/detection_worker.h"
#include "runtime/inspection_plc_controller.h"
#include "runtime/inspection_run_context.h"
#include "runtime/pipeline_registry.h"
#include "runtime/result_presentation_mailbox.h"
#include "runtime/result_service.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

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
    ProductIdentityAmbiguous,
    RuntimeInvariantViolation
};

Q_DECLARE_METATYPE(InspectionFaultReason)

struct InspectionFaultSnapshot
{
    InspectionFaultReason reason = InspectionFaultReason::None;
    QString diagnostic;
    QString runId;
    quint64 acceptedProductCount = 0;
    quint64 completedProductCount = 0;
    quint64 postFaultDroppedFrameCount = 0;
    QDateTime occurredAtUtc;

    bool isActive() const
    {
        return reason != InspectionFaultReason::None;
    }
};

// Owns one complete production run. Frame admission never creates a missing
// run implicitly; beginStart() is the only run creation point.
class InspectionRuntime
{
public:
    using RunIdFactory = std::function<QString()>;

    explicit InspectionRuntime(
        const RunIdFactory &runIdFactory,
        const std::shared_ptr<InspectionPlcController> &plcController,
        const std::shared_ptr<PipelineRegistry> &pipelineRegistry);
    ~InspectionRuntime();

    QString beginStart(
        const MachineSettings &machineSettings,
        const PreparedRecipeSnapshot &preparedRecipe,
        const InspectionProfileSnapshot &profileSnapshot);
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
    int unresolvedFaultProductCount() const;
    int faultUnconfirmedProductCount() const;

    bool hasPlcController() const;
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

    BarcodeRuntimeReadiness preparePipeline(DetectionMode mode) const;
    bool startPipeline(
        const ResultServiceRunConfiguration &resultConfiguration,
        QString *errorMessage);
    std::shared_ptr<const InspectionRunContext> runContext() const;

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
    bool isDetectionWorkerActiveForMode(DetectionMode mode) const;
    DetectionMode detectionWorkerMode() const;
    std::size_t detectionWorkerQueueCapacity() const;
    bool submitDetectionFrame(
        const std::shared_ptr<const FrameData> &frame);
    bool submitDetectionWorkItem(const DetectionWorkItem &item);
    DetectionWorkSubmissionResult trySubmitDetectionFrame(
        const std::shared_ptr<const FrameData> &frame);
    DetectionWorkSubmissionResult trySubmitDetectionWorkItem(
        const DetectionWorkItem &item);

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

private:
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
    int m_faultUnconfirmedProductCount = 0;
    InspectionFaultSnapshot m_faultSnapshot;
    std::map<quint64, std::weak_ptr<const FrameData>> m_acceptedFrames;
    std::map<quint64, ProductProgress> m_products;

    std::shared_ptr<InspectionPlcController> m_plcController;
    std::shared_ptr<PipelineRegistry> m_pipelineRegistry;
    std::unique_ptr<ResultService> m_resultService;

    mutable std::mutex m_detectionWorkerMutex;
    std::shared_ptr<DetectionWorker> m_detectionWorker;
    UiCompletionMailbox m_uiCompletionMailbox;
    std::atomic<bool> m_detectionWorkerActive{false};
    std::atomic<DetectionMode> m_detectionWorkerMode{DetectionMode::Stamp};
};
