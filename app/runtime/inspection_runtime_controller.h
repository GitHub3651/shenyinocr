#pragma once

#include "runtime/detection_session.h"
#include "runtime/result_handler.h"

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

    QString beginStart();
    bool markRunning();
    bool requestStop();
    void finishStop();
    void markFault();
    void acknowledgeFault();

    InspectionRuntimeState state() const;
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
    bool consumeDueDelayedNgRequest();

    DetectionResultStatistics statistics() const;
    int totalCount() const;
    int ngCount() const;
    int pendingDelayedNgCount() const;
    void resetStatistics();
    void resetNgCount();
    void clearPendingDelayedNgRequests();

private:
    InspectionRuntimeState m_state = InspectionRuntimeState::Idle;
    DetectionSession m_session;
    DetectionResultHandler m_resultHandler;
    QString m_lastRecordedRunId;
    quint64 m_lastRecordedProductSequence = 0;
};
