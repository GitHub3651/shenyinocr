#include "runtime/inspection_runtime_stop_transaction.h"

InspectionRuntimeStopTransaction::InspectionRuntimeStopTransaction(
    InspectionRuntimeController &controller)
    : m_controller(controller)
{
}

void InspectionRuntimeStopTransaction::begin()
{
    if (m_hasBegun || m_committed) {
        return;
    }
    m_controller.requestStop();
    m_hasBegun = true;
}

void InspectionRuntimeStopTransaction::waitForDetectionWorker()
{
    if (!m_hasBegun || m_hasWaitedForDetectionWorker
            || m_committed) {
        return;
    }
    m_controller.waitForDetectionWorkerStop();
    m_hasWaitedForDetectionWorker = true;
}

void InspectionRuntimeStopTransaction::commit()
{
    if (!m_hasBegun || m_committed) {
        return;
    }
    m_controller.finishStop();
    m_committed = true;
}

bool InspectionRuntimeStopTransaction::hasBegun() const
{
    return m_hasBegun;
}

bool InspectionRuntimeStopTransaction::hasWaitedForDetectionWorker() const
{
    return m_hasWaitedForDetectionWorker;
}

bool InspectionRuntimeStopTransaction::isCommitted() const
{
    return m_committed;
}
