#include "hikvision_camera_device.h"

namespace {

const int kBackendUnavailable = -1;

} // namespace

struct HikvisionCameraDevice::Impl
{
    HikvisionCameraFunctions functions;
};

HikvisionCameraDevice::HikvisionCameraDevice(
    const HikvisionCameraFunctions &functions)
    : m_impl(new Impl)
{
    m_impl->functions = functions;
}

HikvisionCameraDevice::~HikvisionCameraDevice() = default;

CameraOperationResult HikvisionCameraDevice::enumerateDevices(
    int *deviceCount)
{
    if (!m_impl->functions.enumerateDevices) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(
        m_impl->functions.enumerateDevices(deviceCount));
}

CameraOperationResult HikvisionCameraDevice::openDevice(int deviceIndex)
{
    if (!m_impl->functions.openDevice) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(
        m_impl->functions.openDevice(deviceIndex));
}

CameraOperationResult HikvisionCameraDevice::close()
{
    if (!m_impl->functions.close) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(m_impl->functions.close());
}

CameraOperationResult HikvisionCameraDevice::registerImageCallback()
{
    if (!m_impl->functions.registerImageCallback) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(
        m_impl->functions.registerImageCallback());
}

CameraOperationResult HikvisionCameraDevice::startGrabbing()
{
    if (!m_impl->functions.startGrabbing) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(m_impl->functions.startGrabbing());
}

CameraOperationResult HikvisionCameraDevice::stopGrabbing()
{
    if (!m_impl->functions.stopGrabbing) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(m_impl->functions.stopGrabbing());
}

CameraOperationResult HikvisionCameraDevice::setEnumValue(
    const char *key,
    unsigned int value)
{
    if (!m_impl->functions.setEnumValue) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(
        m_impl->functions.setEnumValue(key, value));
}

CameraOperationResult HikvisionCameraDevice::setFloatValue(
    const char *key,
    float value)
{
    if (!m_impl->functions.setFloatValue) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(
        m_impl->functions.setFloatValue(key, value));
}

CameraOperationResult HikvisionCameraDevice::getFloatValue(
    const char *key,
    CameraFloatValue *value)
{
    if (!m_impl->functions.getFloatValue) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(
        m_impl->functions.getFloatValue(key, value));
}

CameraOperationResult HikvisionCameraDevice::getBoolValue(
    const char *key,
    bool *value)
{
    if (!m_impl->functions.getBoolValue) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(
        m_impl->functions.getBoolValue(key, value));
}

CameraOperationResult HikvisionCameraDevice::executeCommand(
    const char *key)
{
    if (!m_impl->functions.executeCommand) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(m_impl->functions.executeCommand(key));
}

CameraOperationResult HikvisionCameraDevice::readBuffer(cv::Mat &image)
{
    if (!m_impl->functions.readBuffer) {
        return CameraOperationResult(kBackendUnavailable);
    }
    return CameraOperationResult(m_impl->functions.readBuffer(image));
}

cv::Mat HikvisionCameraDevice::latestImage()
{
    if (!m_impl->functions.latestImage) {
        return cv::Mat();
    }
    return m_impl->functions.latestImage();
}

cv::Mat HikvisionCameraDevice::waitForImage()
{
    if (!m_impl->functions.waitForImage) {
        return cv::Mat();
    }
    return m_impl->functions.waitForImage();
}

bool HikvisionCameraDevice::takeImageForMainIfReady(cv::Mat &image)
{
    return m_impl->functions.takeImageForMainIfReady
        && m_impl->functions.takeImageForMainIfReady(image);
}

std::uint64_t HikvisionCameraDevice::frameSequence() const
{
    if (!m_impl->functions.frameSequence) {
        return 0;
    }
    return m_impl->functions.frameSequence();
}

bool HikvisionCameraDevice::isImageReadyForMain()
{
    return m_impl->functions.isImageReadyForMain
        && m_impl->functions.isImageReadyForMain();
}

void HikvisionCameraDevice::setNonBlocking(bool enabled)
{
    if (m_impl->functions.setNonBlocking) {
        m_impl->functions.setNonBlocking(enabled);
    }
}

void HikvisionCameraDevice::deferSwitchToBlockingAfterNextFrame()
{
    if (m_impl->functions.deferSwitchToBlockingAfterNextFrame) {
        m_impl->functions.deferSwitchToBlockingAfterNextFrame();
    }
}

void HikvisionCameraDevice::requestStop()
{
    if (m_impl->functions.requestStop) {
        m_impl->functions.requestStop();
    }
}
