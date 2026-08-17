#include "application/inspection_runtime_port.h"

void InspectionRuntimePort::bind(const Callbacks &callbacks)
{
    m_callbacks = callbacks;
}

BarcodeRuntimeReadiness
InspectionRuntimePort::prepareBarcodeDecoder() const
{
    if (m_callbacks.prepareBarcodeDecoder) {
        return m_callbacks.prepareBarcodeDecoder();
    }
    BarcodeRuntimeReadiness readiness;
    readiness.ready = false;
    readiness.errorMessage = QStringLiteral(
                "Barcode runtime port is not bound.");
    return readiness;
}

bool InspectionRuntimePort::startInspection(
    const InspectionStartExecutionCommand &command,
    InspectionRuntimeStartTransaction &transaction,
    QString *errorMessage) const
{
    if (!m_callbacks.startInspection) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "Inspection runtime port is not bound.");
        }
        return false;
    }
    return m_callbacks.startInspection(
                command, transaction, errorMessage);
}

void InspectionRuntimePort::rollbackStart() const
{
    if (m_callbacks.rollbackStart) {
        m_callbacks.rollbackStart();
    }
}

bool InspectionRuntimePort::reconcileFaultProducts(
    QString *summary,
    QString *errorMessage) const
{
    if (!m_callbacks.reconcileFaultProducts) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "Fault reconciliation port is not bound.");
        }
        return false;
    }
    return m_callbacks.reconcileFaultProducts(summary, errorMessage);
}
