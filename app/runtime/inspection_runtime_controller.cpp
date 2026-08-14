#include "runtime/inspection_runtime_controller.h"

InspectionRuntimeController::InspectionRuntimeController(
    const RunIdFactory &runIdFactory)
    : m_session(runIdFactory)
{
}

QString InspectionRuntimeController::beginStart()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Idle) {
        return QString();
    }

    m_state = InspectionRuntimeState::Starting;
    m_lastRecordedRunId.clear();
    m_lastRecordedProductSequence = 0;
    return m_session.begin();
}

bool InspectionRuntimeController::markRunning()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Starting) {
        return false;
    }

    m_state = InspectionRuntimeState::Running;
    return true;
}

bool InspectionRuntimeController::requestStop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == InspectionRuntimeState::Stopping) {
        return true;
    }
    if (m_state != InspectionRuntimeState::Starting
            && m_state != InspectionRuntimeState::Running) {
        return false;
    }

    m_state = InspectionRuntimeState::Stopping;
    return true;
}

void InspectionRuntimeController::finishStop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = InspectionRuntimeState::Idle;
}

void InspectionRuntimeController::markFault()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = InspectionRuntimeState::Fault;
}

void InspectionRuntimeController::acknowledgeFault()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == InspectionRuntimeState::Fault) {
        m_state = InspectionRuntimeState::Idle;
    }
}

InspectionRuntimeState InspectionRuntimeController::state() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

bool InspectionRuntimeController::isBusy() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == InspectionRuntimeState::Starting
            || m_state == InspectionRuntimeState::Running
            || m_state == InspectionRuntimeState::Stopping;
}

bool InspectionRuntimeController::isRunning() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state == InspectionRuntimeState::Running;
}

QString InspectionRuntimeController::runId() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_session.runId();
}

quint64 InspectionRuntimeController::completedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_session.completedProductCount();
}

quint64 InspectionRuntimeController::acceptedProductCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_session.acceptedProductCount();
}

std::shared_ptr<const FrameData> InspectionRuntimeController::acceptFrame(
    const cv::Mat &image,
    quint64 frameNumber,
    int cameraIndex,
    const QDateTime &timestampUtc)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != InspectionRuntimeState::Starting
            && m_state != InspectionRuntimeState::Running) {
        return std::shared_ptr<const FrameData>();
    }

    return m_session.acceptFrame(
                image,
                frameNumber,
                cameraIndex,
                timestampUtc);
}

DetectionCompletion InspectionRuntimeController::complete(
    const std::shared_ptr<const FrameData> &frame,
    const DetectionResult &result)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == InspectionRuntimeState::Idle
            || m_state == InspectionRuntimeState::Fault) {
        return DetectionCompletion();
    }

    return m_session.complete(frame, result);
}

DetectionCompletion InspectionRuntimeController::complete(
    const cv::Mat &image,
    const DetectionResult &result,
    quint64 frameNumber,
    int cameraIndex,
    const QDateTime &timestampUtc)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == InspectionRuntimeState::Idle
            || m_state == InspectionRuntimeState::Fault) {
        return DetectionCompletion();
    }

    return m_session.complete(
                image,
                result,
                frameNumber,
                cameraIndex,
                timestampUtc);
}

DetectionResultHandlingOutcome InspectionRuntimeController::record(
    const DetectionCompletion &completion,
    int imageSaveModeIndex,
    int delayedNgOffset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    DetectionResultHandlingOutcome rejected;
    rejected.statistics = m_resultHandler.statistics();
    if (m_state == InspectionRuntimeState::Idle
            || m_state == InspectionRuntimeState::Fault
            || !completion.isValid()) {
        return rejected;
    }

    const ProductKey &productKey = completion.frame->productKey;
    if (productKey.runId != m_session.runId()
            || (productKey.runId == m_lastRecordedRunId
                && productKey.sequence
                   <= m_lastRecordedProductSequence)) {
        return rejected;
    }

    const DetectionResultHandlingOutcome outcome = m_resultHandler.record(
                completion,
                imageSaveModeIndex,
                delayedNgOffset);
    if (outcome.resultRecorded) {
        m_lastRecordedRunId = productKey.runId;
        m_lastRecordedProductSequence = productKey.sequence;
    }
    return outcome;
}

bool InspectionRuntimeController::consumeDueDelayedNgRequest()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.consumeDueDelayedNgRequest();
}

DetectionResultStatistics InspectionRuntimeController::statistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.statistics();
}

int InspectionRuntimeController::totalCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.totalCount();
}

int InspectionRuntimeController::ngCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.ngCount();
}

int InspectionRuntimeController::pendingDelayedNgCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resultHandler.pendingDelayedNgCount();
}

void InspectionRuntimeController::resetStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resultHandler.resetStatistics();
}

void InspectionRuntimeController::resetNgCount()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resultHandler.resetNgCount();
}

void InspectionRuntimeController::clearPendingDelayedNgRequests()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resultHandler.clearPendingDelayedNgRequests();
}
