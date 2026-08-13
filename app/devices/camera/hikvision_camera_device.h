#ifndef HIKVISION_CAMERA_DEVICE_H
#define HIKVISION_CAMERA_DEVICE_H

#include "camera_device.h"

#include <functional>
#include <memory>

struct HikvisionCameraFunctions
{
    std::function<int(int *)> enumerateDevices;
    std::function<int(int)> openDevice;
    std::function<int()> close;
    std::function<int()> registerImageCallback;
    std::function<int()> startGrabbing;
    std::function<int()> stopGrabbing;
    std::function<int(const char *, unsigned int)> setEnumValue;
    std::function<int(const char *, float)> setFloatValue;
    std::function<int(const char *, CameraFloatValue *)> getFloatValue;
    std::function<int(const char *, bool *)> getBoolValue;
    std::function<int(const char *)> executeCommand;
    std::function<int(cv::Mat &)> readBuffer;
    std::function<cv::Mat()> latestImage;
    std::function<cv::Mat()> waitForImage;
    std::function<bool(cv::Mat &)> takeImageForMainIfReady;
    std::function<std::uint64_t()> frameSequence;
    std::function<bool()> isImageReadyForMain;
    std::function<void(bool)> setNonBlocking;
    std::function<void()> deferSwitchToBlockingAfterNextFrame;
    std::function<void()> requestStop;
};

class HikvisionCameraDevice final : public ICameraDevice
{
public:
    HikvisionCameraDevice();
    explicit HikvisionCameraDevice(
        const HikvisionCameraFunctions &functions);
    ~HikvisionCameraDevice() override;

    CameraOperationResult enumerateDevices(int *deviceCount) override;
    CameraOperationResult openDevice(int deviceIndex) override;
    CameraOperationResult close() override;
    CameraOperationResult registerImageCallback() override;
    CameraOperationResult startGrabbing() override;
    CameraOperationResult stopGrabbing() override;
    CameraOperationResult setEnumValue(
        const char *key,
        unsigned int value) override;
    CameraOperationResult setFloatValue(
        const char *key,
        float value) override;
    CameraOperationResult getFloatValue(
        const char *key,
        CameraFloatValue *value) override;
    CameraOperationResult getBoolValue(
        const char *key,
        bool *value) override;
    CameraOperationResult executeCommand(const char *key) override;
    CameraOperationResult readBuffer(cv::Mat &image) override;
    cv::Mat latestImage() override;
    cv::Mat waitForImage() override;
    bool takeImageForMainIfReady(cv::Mat &image) override;
    std::uint64_t frameSequence() const override;
    bool isImageReadyForMain() override;
    void setNonBlocking(bool enabled) override;
    void deferSwitchToBlockingAfterNextFrame() override;
    void requestStop() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // HIKVISION_CAMERA_DEVICE_H
