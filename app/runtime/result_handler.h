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

    DetectionResultStatistics statistics() const;
    int totalCount() const;
    int ngCount() const;
    int pendingDelayedNgCount() const;

    void resetStatistics();
    void resetNgCount();
    void clearPendingDelayedNgRequests();

private:
    DetectionResultStatistics m_statistics;
    std::queue<int> m_delayedNgDueCounts;
};
