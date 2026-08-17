#pragma once

#include <QString>

enum class InspectionAcquisitionDto
{
    SoftwareTrigger,
    HardwareTrigger
};

enum class CameraOpenIssueDto
{
    None,
    DeviceNotFound,
    DeviceOpenFailed,
    ExposureFailed,
    InitializationFailed
};

struct CameraParameterResultDto
{
    bool success = false;
    int minimumValue = 0;
    int maximumValue = 0;
    double actualValue = 0.0;
    int nativeErrorCode = 0;
    QString diagnostic;
};

struct CameraOpenResultDto
{
    CameraOpenIssueDto issue = CameraOpenIssueDto::None;
    int deviceCount = 0;
    int appliedExposure = 0;
    int exposureMinimum = 0;
    int exposureMaximum = 0;
    bool exposureAdjusted = false;
    QString adjustmentMessage;
    QString diagnostic;

    bool isSuccess() const
    {
        return issue == CameraOpenIssueDto::None;
    }
};

enum class CameraRecoveryIssueDto
{
    None,
    MissingCamera,
    ExposureRejected,
    InitializationFailed
};

struct CameraRecoveryResultDto
{
    CameraRecoveryIssueDto issue = CameraRecoveryIssueDto::None;
    bool recoveryAttempted = false;
    bool cameraOpen = false;
    QString adjustmentMessage;
    QString errorMessage;

    bool isRecovered() const
    {
        return issue == CameraRecoveryIssueDto::None;
    }
};
