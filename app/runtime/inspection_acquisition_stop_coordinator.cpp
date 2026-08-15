#include "runtime/inspection_acquisition_stop_coordinator.h"

namespace {

void requestStopIfPresent(
    const InspectionAcquisitionStopEndpoint &endpoint)
{
    if (endpoint.isPresent && endpoint.requestStop) {
        endpoint.requestStop();
    }
}

bool isRunning(
    const InspectionAcquisitionStopEndpoint &endpoint)
{
    return endpoint.isPresent
            && endpoint.isRunning
            && endpoint.isRunning();
}

bool waitForEndpoint(
    const InspectionAcquisitionStopEndpoint &endpoint,
    unsigned long waitMilliseconds)
{
    return !endpoint.wait
            || endpoint.wait(waitMilliseconds);
}

} // namespace

InspectionAcquisitionStopResult
InspectionAcquisitionStopCoordinator::stop(
    const InspectionAcquisitionStopRequest &request)
{
    InspectionAcquisitionStopResult result;
    result.hardwareWasPresent = request.hardware.isPresent;

    // Preserve the existing admission-stop order before either wait begins.
    requestStopIfPresent(request.software);
    requestStopIfPresent(request.hardware);

    // The legacy Widget checked the software thread only after both stop
    // requests had been delivered.  Keep that detail so an already-finished
    // software worker does not cause an unnecessary camera recovery.
    result.softwareWasRunning = isRunning(request.software);

    if (result.softwareWasRunning) {
        if (request.software.stopLoop) {
            request.software.stopLoop();
        }
        result.softwareStopped = waitForEndpoint(
                    request.software,
                    request.softwareWaitMilliseconds);
        if (result.softwareStopped
                && request.software.afterStopped) {
            request.software.afterStopped();
        }
    }

    if (result.hardwareWasPresent) {
        if (request.hardware.prepareForWait) {
            request.hardware.prepareForWait();
        }
        if (request.hardware.repeatStopRequestAfterPrepare) {
            requestStopIfPresent(request.hardware);
        }

        const bool hardwareWasRunning = isRunning(request.hardware);
        if (hardwareWasRunning
                || request.hardware.waitWhenPresent) {
            result.hardwareStopped = waitForEndpoint(
                        request.hardware,
                        request.hardwareWaitMilliseconds);
        }
        if (result.hardwareStopped
                && request.hardware.afterStopped) {
            request.hardware.afterStopped();
        }
    }

    return result;
}
