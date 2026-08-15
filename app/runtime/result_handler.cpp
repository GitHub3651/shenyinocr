#include "runtime/result_handler.h"

double DetectionResultStatistics::passRatePercent() const
{
    return totalCount > 0
            ? (1.0 - static_cast<double>(ngCount) / totalCount) * 100.0
            : 0.0;
}

DetectionResultSaveAction DetectionResultHandler::imageSaveActionFor(
    AlgorithmVerdict verdict,
    int imageSaveModeIndex)
{
    const bool isOk = verdict == AlgorithmVerdict::Ok;
    if (isOk) {
        return imageSaveModeIndex == 2 || imageSaveModeIndex == 3
                ? DetectionResultSaveAction::SaveOk
                : DetectionResultSaveAction::DoNotSave;
    }

    return imageSaveModeIndex == 1 || imageSaveModeIndex == 3
            ? DetectionResultSaveAction::SaveNg
            : DetectionResultSaveAction::DoNotSave;
}

DetectionResultHandlingOutcome DetectionResultHandler::record(
    const DetectionCompletion &completion,
    int imageSaveModeIndex,
    int delayedNgOffset)
{
    DetectionResultHandlingOutcome outcome;
    outcome.statistics = m_statistics;
    if (!completion.isValid()) {
        return outcome;
    }

    outcome.resultRecorded = true;
    outcome.imageSaveAction = imageSaveActionFor(
                completion.result.verdict,
                imageSaveModeIndex);

    ++m_statistics.totalCount;
    if (completion.result.verdict == AlgorithmVerdict::Ok) {
        outcome.plcAction = DetectionPlcAction::RequestOk;
    } else {
        ++m_statistics.ngCount;
        if (delayedNgOffset == 0) {
            outcome.plcAction = DetectionPlcAction::RequestNg;
        } else {
            m_delayedNgDueCounts.push(
                        m_statistics.totalCount + delayedNgOffset);
        }
    }

    outcome.statistics = m_statistics;
    return outcome;
}

bool DetectionResultHandler::consumeDueDelayedNgRequest()
{
    if (m_delayedNgDueCounts.empty()
            || m_statistics.totalCount
               < m_delayedNgDueCounts.front() - 1) {
        return false;
    }

    m_delayedNgDueCounts.pop();
    return true;
}

void DetectionResultHandler::recordSystemFault()
{
    ++m_abnormalStatistics.systemFaultCount;
}

void DetectionResultHandler::recordCancelledProduct()
{
    ++m_abnormalStatistics.cancelledProductCount;
}

void DetectionResultHandler::recordUnconfirmedProduct()
{
    ++m_abnormalStatistics.unconfirmedProductCount;
}

void DetectionResultHandler::recordPostFaultDroppedFrame()
{
    ++m_abnormalStatistics.postFaultDroppedFrameCount;
}

DetectionResultStatistics DetectionResultHandler::statistics() const
{
    return m_statistics;
}

DetectionAbnormalStatistics DetectionResultHandler::abnormalStatistics() const
{
    return m_abnormalStatistics;
}

int DetectionResultHandler::totalCount() const
{
    return m_statistics.totalCount;
}

int DetectionResultHandler::ngCount() const
{
    return m_statistics.ngCount;
}

int DetectionResultHandler::pendingDelayedNgCount() const
{
    return static_cast<int>(m_delayedNgDueCounts.size());
}

void DetectionResultHandler::resetStatistics()
{
    m_statistics = DetectionResultStatistics();
}

void DetectionResultHandler::resetAbnormalStatistics()
{
    m_abnormalStatistics = DetectionAbnormalStatistics();
}

void DetectionResultHandler::resetNgCount()
{
    m_statistics.ngCount = 0;
}

void DetectionResultHandler::clearPendingDelayedNgRequests()
{
    std::queue<int> empty;
    m_delayedNgDueCounts.swap(empty);
}
