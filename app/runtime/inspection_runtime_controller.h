#pragma once

#include "runtime/detection_worker.h"
#include "runtime/detection_session.h"
#include "runtime/inspection_fault_state.h"
#include "runtime/inspection_product_reconciler.h"
#include "runtime/result_handler.h"
#include "runtime/result_presentation_mailbox.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <set>

enum class InspectionRuntimeState {
    Idle,
    Starting,
    Running,
    Stopping,
    Fault
};

class InspectionRuntimeController
{
public:
    using RunIdFactory = DetectionSession::RunIdFactory;

    explicit InspectionRuntimeController(
        const RunIdFactory &runIdFactory = RunIdFactory());
    ~InspectionRuntimeController();

    QString beginStart();
    bool markRunning();
    bool requestStop();
    void finishStop();
    bool enterFault(
        InspectionFaultReason reason,
        const QString &diagnostic = QString(),
        const QDateTime &occurredAtUtc = QDateTime());
    bool acknowledgeFault();

    InspectionRuntimeState state() const;
    InspectionFaultSnapshot faultSnapshot() const;
    bool isBusy() const;
    bool isRunning() const;
    QString runId() const;
    quint64 acceptedProductCount() const;
    quint64 completedProductCount() const;

    std::shared_ptr<const FrameData> acceptFrame(
        const cv::Mat &image,
        quint64 frameNumber = 0,
        int cameraIndex = 0,
        const QDateTime &timestampUtc = QDateTime());

    DetectionCompletion complete(
        const std::shared_ptr<const FrameData> &frame,
        const DetectionResult &result);

    DetectionCompletion complete(
        const cv::Mat &image,
        const DetectionResult &result,
        quint64 frameNumber = 0,
        int cameraIndex = 0,
        const QDateTime &timestampUtc = QDateTime());

    DetectionResultHandlingOutcome record(
        const DetectionCompletion &completion,
        int imageSaveModeIndex,
        int delayedNgOffset);
    bool consumeDueDelayedNgRequest(ProductKey *productKey = nullptr);

    std::vector<InspectionFaultProductAction> faultProductActions(
        bool plcWritable) const;
    bool resolveFaultProduct(
        const ProductKey &productKey,
        InspectionFaultProductResolution resolution);
    bool recordFaultedPlcOutput(const ProductKey &productKey);
    int unresolvedFaultProductCount() const;
    int faultedPlcOutputCount() const;
    int faultFallbackNgResolutionCount() const;
    int faultUnconfirmedProductCount() const;

    DetectionResultStatistics statistics() const;
    DetectionAbnormalStatistics abnormalStatistics() const;
    int totalCount() const;
    int ngCount() const;
    int pendingDelayedNgCount() const;
    void resetStatistics();
    void resetAbnormalStatistics();
    void resetNgCount();
    void clearPendingDelayedNgRequests();

    bool startDetectionWorker(
        int modeIndex,
        const std::shared_ptr<DetectionWorker> &worker);
    void requestDetectionWorkerStop();
    void waitForDetectionWorkerStop();
    bool isDetectionWorkerActive() const;
    bool isDetectionWorkerActiveForMode(int modeIndex) const;
    int detectionWorkerModeIndex() const;
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

private:
    mutable std::mutex m_mutex;
    InspectionRuntimeState m_state = InspectionRuntimeState::Idle;
    DetectionSession m_session;
    InspectionFaultState m_faultState;
    InspectionProductReconciler m_productReconciler;
    DetectionResultHandler m_resultHandler;
    QString m_lastRecordedRunId;
    quint64 m_lastRecordedProductSequence = 0;
    std::set<quint64> m_faultedPlcOutputSequences;
    int m_faultFallbackNgResolutionCount = 0;
    int m_faultUnconfirmedProductCount = 0;

    mutable std::mutex m_detectionWorkerMutex;
    std::shared_ptr<DetectionWorker> m_detectionWorker;
    UiCompletionMailbox m_uiCompletionMailbox;
    std::atomic<int> m_detectionWorkerModeIndex;
};
