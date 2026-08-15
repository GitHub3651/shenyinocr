#pragma once

#include "TrackingTypes.h"

#include <queue>

enum class DetectionResultSaveAction {
    DoNotSave,
    SaveOk,
    SaveNg
};

enum class DetectionPlcAction {
    NoRequest,
    RequestOk,
    RequestNg
};

struct DetectionResultStatistics {
    int totalCount = 0;
    int ngCount = 0;

    double passRatePercent() const;
};

struct DetectionAbnormalStatistics {
    quint64 systemFaultCount = 0;
    quint64 cancelledProductCount = 0;
    quint64 unconfirmedProductCount = 0;
    quint64 postFaultDroppedFrameCount = 0;
};

struct DetectionResultHandlingOutcome {
    bool resultRecorded = false;
    DetectionResultSaveAction imageSaveAction =
            DetectionResultSaveAction::DoNotSave;
    DetectionPlcAction plcAction = DetectionPlcAction::NoRequest;
    DetectionResultStatistics statistics;
};

class DetectionResultHandler
{
public:
    static DetectionResultSaveAction imageSaveActionFor(
        AlgorithmVerdict verdict,
        int imageSaveModeIndex);

    DetectionResultHandlingOutcome record(
        const DetectionCompletion &completion,
        int imageSaveModeIndex,
        int delayedNgOffset);

    bool consumeDueDelayedNgRequest();
    void recordSystemFault();
    void recordCancelledProduct();
    void recordUnconfirmedProduct();
    void recordPostFaultDroppedFrame();

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
    DetectionResultStatistics m_statistics;
    DetectionAbnormalStatistics m_abnormalStatistics;
    std::queue<int> m_delayedNgDueCounts;
};
