#pragma once

#include <functional>

struct InspectionAcquisitionStopEndpoint
{
    bool isPresent = false;
    bool waitWhenPresent = false;
    bool repeatStopRequestAfterPrepare = false;
    std::function<bool()> isRunning;
    std::function<void()> requestStop;
    std::function<void()> stopLoop;
    std::function<void()> prepareForWait;
    std::function<bool(unsigned long)> wait;
    std::function<void()> afterStopped;
};

struct InspectionAcquisitionStopRequest
{
    InspectionAcquisitionStopEndpoint software;
    InspectionAcquisitionStopEndpoint hardware;
    unsigned long softwareWaitMilliseconds = 3000UL;
    unsigned long hardwareWaitMilliseconds = 3000UL;
};

struct InspectionAcquisitionStopResult
{
    bool softwareWasRunning = false;
    bool hardwareWasPresent = false;
    bool softwareStopped = true;
    bool hardwareStopped = true;

    bool allStopped() const
    {
        return softwareStopped && hardwareStopped;
    }

    bool shouldRestoreCamera() const
    {
        return softwareWasRunning || hardwareWasPresent;
    }
};

class InspectionAcquisitionStopCoordinator
{
public:
    static InspectionAcquisitionStopResult stop(
        const InspectionAcquisitionStopRequest &request);
};
