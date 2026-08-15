#include "runtime/inspection_runtime_start_transaction.h"

InspectionRuntimeStartTransaction::InspectionRuntimeStartTransaction(
    InspectionRuntimeController &controller)
    : m_controller(controller)
{
}

InspectionRuntimeStartTransaction::~InspectionRuntimeStartTransaction()
{
    rollback();
}

bool InspectionRuntimeStartTransaction::begin()
{
    if (m_hasBegun || m_committed) {
        return false;
    }

    m_runId = m_controller.beginStart();
    m_hasBegun = !m_runId.isEmpty();
    return m_hasBegun;
}

bool InspectionRuntimeStartTransaction::startDetectionWorker(
    int modeIndex,
    const std::shared_ptr<DetectionWorker> &worker)
{
    return m_hasBegun
            && !m_committed
            && m_controller.startDetectionWorker(modeIndex, worker);
}

bool InspectionRuntimeStartTransaction::commit()
{
    if (!m_hasBegun || m_committed
            || !m_controller.markRunning()) {
        return false;
    }

    m_committed = true;
    return true;
}

void InspectionRuntimeStartTransaction::rollback()
{
    if (!m_hasBegun || m_committed) {
        return;
    }

    m_controller.requestStop();
    m_controller.waitForDetectionWorkerStop();
    m_controller.finishStop();
    m_hasBegun = false;
    m_runId.clear();
}

bool InspectionRuntimeStartTransaction::hasBegun() const
{
    return m_hasBegun;
}

bool InspectionRuntimeStartTransaction::isCommitted() const
{
    return m_committed;
}

QString InspectionRuntimeStartTransaction::runId() const
{
    return m_runId;
}
