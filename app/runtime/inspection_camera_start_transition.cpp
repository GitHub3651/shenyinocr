#include "runtime/inspection_camera_start_transition.h"

#include <QThread>

namespace {

void delayFor(
    const InspectionCameraStartRequest &request,
    unsigned long milliseconds)
{
    if (request.delayMilliseconds) {
        request.delayMilliseconds(milliseconds);
        return;
    }
    QThread::msleep(milliseconds);
}

InspectionCameraStartResult applyExposure(
    const InspectionCameraStartRequest &request)
{
    InspectionCameraStartResult result;
    if (!request.applyExposure
            || !request.applyExposure(&result.errorMessage)) {
        result.issue = InspectionCameraStartIssue::ExposureRejected;
    }
    return result;
}

} // namespace

InspectionCameraStartResult InspectionCameraStartTransition::apply(
    const InspectionCameraStartRequest &request)
{
    InspectionCameraStartResult result;
    if (!request.cameraDevice) {
        result.issue = InspectionCameraStartIssue::MissingCamera;
        return result;
    }

    if (request.acquisitionKind
            == InspectionAcquisitionKind::HardwareTrigger) {
        try {
            request.cameraDevice->stopGrabbing();
            delayFor(request, 200UL);
            request.cameraDevice->setEnumValue("TriggerMode", 1U);
            request.cameraDevice->setEnumValue("TriggerSource", 0U);

            result = applyExposure(request);
            if (!result.isAccepted()) {
                return result;
            }

            request.cameraDevice->setFloatValue("Gain", request.gain);
            request.cameraDevice->setFloatValue("TriggerDelay", 0.0f);
            request.cameraDevice->registerImageCallback();
            request.cameraDevice->startGrabbing();
            request.cameraDevice->setEnumValue(
                        "LineDebouncerTime",
                        5000U);
            delayFor(request, 100UL);
            return result;
        } catch (...) {
            result.issue = InspectionCameraStartIssue::InitializationFailed;
            return result;
        }
    }

    request.cameraDevice->setEnumValue("TriggerSource", 7U);
    result = applyExposure(request);
    if (!result.isAccepted()) {
        return result;
    }
    request.cameraDevice->setFloatValue("Gain", request.gain);
    return result;
}
