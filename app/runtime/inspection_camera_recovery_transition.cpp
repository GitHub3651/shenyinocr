#include "runtime/inspection_camera_recovery_transition.h"

#include <QThread>

namespace {

void delayFor(
    const InspectionCameraRecoveryRequest &request,
    unsigned long milliseconds)
{
    if (request.delayMilliseconds) {
        request.delayMilliseconds(milliseconds);
        return;
    }
    QThread::msleep(milliseconds);
}

} // namespace

InspectionCameraRecoveryResult
InspectionCameraRecoveryTransition::apply(
    const InspectionCameraRecoveryRequest &request)
{
    InspectionCameraRecoveryResult result;
    result.cameraOpen = request.cameraWasOpen;
    if (!request.recoveryRequired) {
        return result;
    }
    result.recoveryAttempted = true;
    if (!request.cameraDevice) {
        result.issue = InspectionCameraRecoveryIssue::MissingCamera;
        return result;
    }

    try {
        request.cameraDevice->close();
        result.cameraOpen = false;
        delayFor(request, 100UL);

        const CameraOperationResult openResult =
                request.cameraDevice->openDevice(0);
        if (!openResult.isSuccess()) {
            result.issue = InspectionCameraRecoveryIssue::OpenFailed;
            return result;
        }

        result.cameraOpen = true;
        request.cameraDevice->setEnumValue("TriggerMode", 1U);
        request.cameraDevice->setEnumValue("TriggerSource", 7U);
        if (!request.applySavedExposure
                || !request.applySavedExposure(
                    &result.adjustmentMessage,
                    &result.errorMessage)) {
            request.cameraDevice->close();
            result.cameraOpen = false;
            result.issue =
                    InspectionCameraRecoveryIssue::ExposureRejected;
            return result;
        }

        request.cameraDevice->setFloatValue("TriggerDelay", 0.0f);
        request.cameraDevice->registerImageCallback();
        request.cameraDevice->startGrabbing();
    } catch (...) {
        result.issue =
                InspectionCameraRecoveryIssue::InitializationFailed;
    }
    return result;
}
