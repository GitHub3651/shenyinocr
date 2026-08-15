#pragma once

#include "devices/camera/camera_device.h"
#include "runtime/inspection_run_configuration.h"

#include <QString>

#include <functional>

enum class InspectionCameraStartIssue {
    None,
    MissingCamera,
    ExposureRejected,
    InitializationFailed
};

struct InspectionCameraStartRequest
{
    ICameraDevice *cameraDevice = nullptr;
    InspectionAcquisitionKind acquisitionKind =
            InspectionAcquisitionKind::SoftwareTrigger;
    float gain = 0.0f;
    std::function<bool(QString *)> applyExposure;
    std::function<void(unsigned long)> delayMilliseconds;
};

struct InspectionCameraStartResult
{
    InspectionCameraStartIssue issue =
            InspectionCameraStartIssue::None;
    QString errorMessage;

    bool isAccepted() const
    {
        return issue == InspectionCameraStartIssue::None;
    }
};

class InspectionCameraStartTransition
{
public:
    static InspectionCameraStartResult apply(
        const InspectionCameraStartRequest &request);
};
