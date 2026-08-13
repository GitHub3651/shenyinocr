#include "hikvision_camera_device.h"

#include "cmvcamera.h"

#include <cstring>
#include <memory>

namespace {

struct NativeCameraState
{
    NativeCameraState()
    {
        std::memset(&deviceList, 0, sizeof(deviceList));
    }

    MV_CC_DEVICE_INFO_LIST deviceList;
    std::unique_ptr<CMvCamera> camera;
};

int unavailableCameraError()
{
    return MV_E_HANDLE;
}

HikvisionCameraFunctions nativeCameraFunctions()
{
    const std::shared_ptr<NativeCameraState> state(
        new NativeCameraState);
    HikvisionCameraFunctions functions;

    functions.enumerateDevices = [state](int *deviceCount) {
        std::memset(&state->deviceList, 0, sizeof(state->deviceList));
        const int result = CMvCamera::EnumDevices(
            MV_GIGE_DEVICE | MV_USB_DEVICE,
            &state->deviceList);
        if (deviceCount) {
            *deviceCount = result == MV_OK
                ? static_cast<int>(state->deviceList.nDeviceNum)
                : 0;
        }
        return result;
    };

    functions.openDevice = [state](int deviceIndex) {
        if (deviceIndex < 0
                || deviceIndex
                   >= static_cast<int>(state->deviceList.nDeviceNum)) {
            return static_cast<int>(MV_E_PARAMETER);
        }
        if (state->camera) {
            return static_cast<int>(MV_E_CALLORDER);
        }

        state->camera.reset(new CMvCamera);
        const int result = state->camera->Open(
            state->deviceList.pDeviceInfo[deviceIndex]);
        if (result != MV_OK) {
            state->camera.reset();
        }
        return result;
    };

    functions.close = [state]() {
        if (!state->camera) {
            return unavailableCameraError();
        }
        const int result = state->camera->Close();
        state->camera.reset();
        return result;
    };

    functions.registerImageCallback = [state]() {
        return state->camera
            ? state->camera->RegisterImageCallBack()
            : unavailableCameraError();
    };
    functions.startGrabbing = [state]() {
        return state->camera
            ? state->camera->StartGrabbing()
            : unavailableCameraError();
    };
    functions.stopGrabbing = [state]() {
        return state->camera
            ? state->camera->StopGrabbing()
            : unavailableCameraError();
    };
    functions.setEnumValue =
        [state](const char *key, unsigned int value) {
        return state->camera
            ? state->camera->SetEnumValue(key, value)
            : unavailableCameraError();
    };
    functions.setFloatValue = [state](const char *key, float value) {
        return state->camera
            ? state->camera->SetFloatValue(key, value)
            : unavailableCameraError();
    };
    functions.getFloatValue =
        [state](const char *key, CameraFloatValue *value) {
        if (!state->camera || !value) {
            return unavailableCameraError();
        }
        MVCC_FLOATVALUE nativeValue = {0};
        const int result = state->camera->GetFloatValue(
            key,
            &nativeValue);
        if (result == MV_OK) {
            value->currentValue = nativeValue.fCurValue;
            value->minimumValue = nativeValue.fMin;
            value->maximumValue = nativeValue.fMax;
        }
        return result;
    };
    functions.getBoolValue = [state](const char *key, bool *value) {
        return state->camera
            ? state->camera->GetBoolValue(key, value)
            : unavailableCameraError();
    };
    functions.executeCommand = [state](const char *key) {
        return state->camera
            ? state->camera->CommandExecute(key)
            : unavailableCameraError();
    };
    functions.readBuffer = [state](cv::Mat &image) {
        return state->camera
            ? state->camera->ReadBuffer(image)
            : unavailableCameraError();
    };
    functions.latestImage = [state]() {
        return state->camera
            ? state->camera->GetImage()
            : cv::Mat();
    };
    functions.waitForImage = [state]() {
        return state->camera
            ? state->camera->timesGetImage()
            : cv::Mat();
    };
    functions.takeImageForMainIfReady =
        [state](cv::Mat &image) {
        return state->camera
            && state->camera->takeImageForMainIfReady(image);
    };
    functions.frameSequence = [state]() -> std::uint64_t {
        return state->camera
            ? state->camera->m_frameseq.load()
            : 0;
    };
    functions.isImageReadyForMain = [state]() {
        return state->camera
            && state->camera->isImageReadyForMain();
    };
    functions.setNonBlocking = [state](bool enabled) {
        if (state->camera) {
            state->camera->setnonblocking(enabled);
        }
    };
    functions.deferSwitchToBlockingAfterNextFrame = [state]() {
        if (state->camera) {
            state->camera->deferswitchtoblockingafternextframe();
        }
    };
    functions.requestStop = [state]() {
        if (state->camera) {
            state->camera->requestStop();
        }
    };

    return functions;
}

} // namespace

HikvisionCameraDevice::HikvisionCameraDevice()
    : HikvisionCameraDevice(nativeCameraFunctions())
{
}
