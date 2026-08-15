#pragma once

#include "devices/camera/camera_device.h"

#include <QString>

#include <functional>

enum class InspectionCameraRecoveryIssue {
    None,
    MissingCamera,
    OpenFailed,
    ExposureRejected,
    InitializationFailed
};

struct InspectionCameraRecoveryRequest
{
    ICameraDevice *cameraDevice = nullptr;
    bool recoveryRequired = false;
    bool cameraWasOpen = false;
    std::function<bool(QString *, QString *)> applySavedExposure;
    std::function<void(unsigned long)> delayMilliseconds;
};

struct InspectionCameraRecoveryResult
{
    InspectionCameraRecoveryIssue issue =
            InspectionCameraRecoveryIssue::None;
    bool recoveryAttempted = false;
    bool cameraOpen = false;
    QString adjustmentMessage;
    QString errorMessage;

    bool isRecovered() const
    {
        return issue == InspectionCameraRecoveryIssue::None;
    }
};

class InspectionCameraRecoveryTransition
{
public:
    static InspectionCameraRecoveryResult apply(
        const InspectionCameraRecoveryRequest &request);
};
