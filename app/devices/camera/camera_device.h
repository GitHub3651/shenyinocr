#pragma once

#include <QDateTime>

#include <opencv2/core.hpp>

#include <cstdint>

enum class CameraResultCode
{
    Success,
    InvalidState,
    InvalidSettings,
    DeviceError
};

struct CameraSettings
{
    bool updateExposure = false;
    float exposure = 0.0f;
    bool updateGain = false;
    float gain = 0.0f;
    bool updateTriggerDelay = false;
    float triggerDelay = 0.0f;
    bool updateLineDebouncerTime = false;
    unsigned int lineDebouncerTime = 5000U;
};

struct CameraSettingRange
{
    float minimum = 0.0f;
    float maximum = 0.0f;
    float current = 0.0f;
};

struct CameraResult
{
    CameraResultCode code = CameraResultCode::Success;
    int nativeErrorCode = 0;
    CameraSettingRange exposureRange;
    CameraSettingRange gainRange;

    bool isSuccess() const
    {
        return code == CameraResultCode::Success;
    }

    static CameraResult deviceError(int nativeErrorCode)
    {
        CameraResult result;
        result.code = CameraResultCode::DeviceError;
        result.nativeErrorCode = nativeErrorCode;
        return result;
    }
};

enum class CameraTriggerMode
{
    Software,
    HardwareLine0
};

struct CameraFrame
{
    std::uint64_t sequence = 0;
    QDateTime timestampUtc;
    cv::Mat image;
};

enum class CameraFrameStatus
{
    FrameReady,
    Timeout,
    Interrupted,
    DeviceError
};

struct CameraFrameResult
{
    CameraFrameStatus status = CameraFrameStatus::Timeout;
    CameraFrame frame;
    int nativeErrorCode = 0;
};

class ICameraDevice
{
public:
    virtual ~ICameraDevice() {}

    virtual CameraResult enumerate(int *deviceCount) = 0;
    virtual CameraResult openFirst() = 0;
    virtual CameraResult applySettings(
        const CameraSettings &settings) = 0;
    virtual CameraResult setTriggerMode(CameraTriggerMode mode) = 0;
    virtual CameraResult startGrabbing() = 0;
    virtual CameraResult triggerSoftware() = 0;
    virtual CameraFrameResult waitNextFrame(int timeoutMs) = 0;
    virtual void interruptWait() = 0;
    virtual CameraResult stopGrabbing() = 0;
    virtual CameraResult close() = 0;
};
