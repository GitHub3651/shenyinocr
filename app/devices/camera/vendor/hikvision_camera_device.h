#pragma once

#include "devices/camera/camera_device.h"

#include <memory>

class HikvisionCameraDevice final : public ICameraDevice
{
public:
    HikvisionCameraDevice();
    ~HikvisionCameraDevice() override;

    CameraResult enumerate(int *deviceCount) override;
    CameraResult openFirst() override;
    CameraResult applySettings(
        const CameraSettings &settings) override;
    CameraResult setTriggerMode(CameraTriggerMode mode) override;
    CameraResult startGrabbing() override;
    CameraResult triggerSoftware() override;
    CameraFrameResult waitNextFrame(int timeoutMs) override;
    void interruptWait() override;
    CameraResult stopGrabbing() override;
    CameraResult close() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
