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

InspectionAcquisitionStopResult
InspectionRuntimePort::stopAcquisition() const
{
    if (m_callbacks.stopAcquisition) {
        return m_callbacks.stopAcquisition();
    }
    InspectionAcquisitionStopResult result;
    result.softwareStopped = false;
    result.hardwareStopped = false;
    return result;
}

InspectionCameraRecoveryResult InspectionRuntimePort::recoverCamera(
    bool recoveryRequired,
    bool cameraWasOpen,
    const MachineSettings &settings,
    const PersistAdjustedExposure &persistAdjustedExposure) const
{
    if (m_callbacks.recoverCamera) {
        return m_callbacks.recoverCamera(
                    recoveryRequired,
                    cameraWasOpen,
                    settings,
                    persistAdjustedExposure);
    }
    InspectionCameraRecoveryResult result;
    result.issue = InspectionCameraRecoveryIssue::MissingCamera;
    result.errorMessage = QStringLiteral(
                "Inspection runtime camera recovery is not bound.");
    return result;
}

InspectionCameraOpenResult InspectionRuntimePort::openCamera(
    const MachineSettings &settings,
    const PersistAdjustedExposure &persistAdjustedExposure) const
{
    if (m_callbacks.openCamera) {
        return m_callbacks.openCamera(
                    settings, persistAdjustedExposure);
    }
    InspectionCameraOpenResult result;
    result.issue = InspectionCameraOpenIssue::CameraUnavailable;
    result.diagnostic = QStringLiteral(
                "Inspection runtime camera port is not bound.");
    return result;
}

void InspectionRuntimePort::closeCamera() const
{
    if (m_callbacks.closeCamera) {
        m_callbacks.closeCamera();
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
