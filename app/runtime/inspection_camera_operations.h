#pragma once

#include "devices/camera/camera_device.h"

#include <QString>

#include <functional>

enum class InspectionCameraOpenIssue
{
    None,
    CameraUnavailable,
    DeviceNotFound,
    DeviceOpenFailed,
    ExposureFailed
};

struct InspectionCameraParameterResult
{
    bool success = false;
    int minimumValue = 0;
    int maximumValue = 0;
    double actualValue = 0.0;
    int nativeErrorCode = 0;
    QString diagnostic;
};

struct InspectionCameraOpenResult
{
    InspectionCameraOpenIssue issue = InspectionCameraOpenIssue::None;
    int deviceCount = 0;
    int appliedExposure = 0;
    int exposureMinimum = 0;
    int exposureMaximum = 0;
    bool exposureAdjusted = false;
    QString adjustmentMessage;
    QString diagnostic;

    bool isSuccess() const
    {
        return issue == InspectionCameraOpenIssue::None;
    }
};

class InspectionCameraOperations
{
public:
    explicit InspectionCameraOperations(ICameraDevice *cameraDevice);
    void setCameraDevice(ICameraDevice *cameraDevice);

    InspectionCameraParameterResult queryExposureRange() const;
    InspectionCameraParameterResult queryGainRange() const;
    InspectionCameraParameterResult applyExposure(int exposureValue) const;
    InspectionCameraParameterResult applyGain(int gainValue) const;
    InspectionCameraParameterResult applySavedExposure(
        int savedExposure,
        const std::function<bool(int, QString *)> &persistAdjustedExposure) const;
    InspectionCameraOpenResult openFirstCamera(
        int savedExposure,
        const std::function<bool(int, QString *)> &persistAdjustedExposure,
        const std::function<void()> &ensureWorkersReady,
        int knownDeviceCount = -1) const;

private:
    ICameraDevice *m_cameraDevice = nullptr;
};
