#include "runtime/inspection_fault_state.h"

bool InspectionFaultSnapshot::isActive() const
{
    return reason != InspectionFaultReason::None;
}

bool InspectionFaultState::enter(
    InspectionFaultReason reason,
    const QString &diagnostic,
    const QString &runId,
    quint64 acceptedProductCount,
    quint64 completedProductCount,
    const QDateTime &occurredAtUtc)
{
    if (reason == InspectionFaultReason::None || isActive()) {
        return false;
    }

    m_snapshot.reason = reason;
    m_snapshot.diagnostic = diagnostic.trimmed();
    m_snapshot.runId = runId;
    m_snapshot.acceptedProductCount = acceptedProductCount;
    m_snapshot.completedProductCount = completedProductCount;
    m_snapshot.postFaultDroppedFrameCount = 0;
    m_snapshot.occurredAtUtc = occurredAtUtc.isValid()
            ? occurredAtUtc.toUTC()
            : QDateTime::currentDateTimeUtc();
    return true;
}

bool InspectionFaultState::notePostFaultDroppedFrame()
{
    if (!isActive()) {
        return false;
    }

    ++m_snapshot.postFaultDroppedFrameCount;
    return true;
}

bool InspectionFaultState::clear()
{
    if (!isActive()) {
        return false;
    }

    m_snapshot = InspectionFaultSnapshot();
    return true;
}

bool InspectionFaultState::isActive() const
{
    return m_snapshot.isActive();
}

InspectionFaultSnapshot InspectionFaultState::snapshot() const
{
    return m_snapshot;
}
