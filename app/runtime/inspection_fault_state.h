#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

enum class InspectionFaultReason {
    None,
    PlcDisconnected,
    HardTriggerQueueOverflow,
    ProductIdentityAmbiguous,
    RuntimeInvariantViolation
};

Q_DECLARE_METATYPE(InspectionFaultReason)

struct InspectionFaultSnapshot {
    InspectionFaultReason reason = InspectionFaultReason::None;
    QString diagnostic;
    QString runId;
    quint64 acceptedProductCount = 0;
    quint64 completedProductCount = 0;
    quint64 postFaultDroppedFrameCount = 0;
    QDateTime occurredAtUtc;

    bool isActive() const;
};

class InspectionFaultState
{
public:
    bool enter(
        InspectionFaultReason reason,
        const QString &diagnostic,
        const QString &runId,
        quint64 acceptedProductCount,
        quint64 completedProductCount,
        const QDateTime &occurredAtUtc = QDateTime());

    bool notePostFaultDroppedFrame();
    bool clear();

    bool isActive() const;
    InspectionFaultSnapshot snapshot() const;

private:
    InspectionFaultSnapshot m_snapshot;
};
