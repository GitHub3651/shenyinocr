#ifndef CAMERA_DEVICE_H
#define CAMERA_DEVICE_H

#include <cstdint>

#include <opencv2/core.hpp>

struct CameraOperationResult
{
    explicit CameraOperationResult(int errorCode = 0)
        : nativeErrorCode(errorCode)
    {
    }

    bool isSuccess() const
    {
        return nativeErrorCode == 0;
    }

    int nativeErrorCode;
};

struct CameraFloatValue
{
    float currentValue = 0.0f;
    float minimumValue = 0.0f;
    float maximumValue = 0.0f;
};

class ICameraDevice
{
public:
    virtual ~ICameraDevice() = default;

    virtual CameraOperationResult enumerateDevices(int *deviceCount) = 0;
    virtual CameraOperationResult openDevice(int deviceIndex) = 0;
    virtual CameraOperationResult close() = 0;

    virtual CameraOperationResult registerImageCallback() = 0;
    virtual CameraOperationResult startGrabbing() = 0;
    virtual CameraOperationResult stopGrabbing() = 0;

    virtual CameraOperationResult setEnumValue(
        const char *key,
        unsigned int value) = 0;
    virtual CameraOperationResult setFloatValue(
        const char *key,
        float value) = 0;
    virtual CameraOperationResult getFloatValue(
        const char *key,
        CameraFloatValue *value) = 0;
    virtual CameraOperationResult getBoolValue(
        const char *key,
        bool *value) = 0;
    virtual CameraOperationResult executeCommand(const char *key) = 0;

    virtual CameraOperationResult readBuffer(cv::Mat &image) = 0;
    virtual cv::Mat latestImage() = 0;
    virtual cv::Mat waitForImage() = 0;
    virtual bool takeImageForMainIfReady(cv::Mat &image) = 0;
    virtual std::uint64_t frameSequence() const = 0;
    virtual bool isImageReadyForMain() = 0;

    virtual void setNonBlocking(bool enabled) = 0;
    virtual void deferSwitchToBlockingAfterNextFrame() = 0;
    virtual void requestStop() = 0;
};

#endif // CAMERA_DEVICE_H
