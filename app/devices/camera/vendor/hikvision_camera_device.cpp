// 文件作用：本文件用于封装海康相机SDK，完成枚举、打开、触发、参数设置和帧回调转换。
// 主要职责：封装海康相机SDK，完成枚举、打开、触发、参数设置和帧回调转换。
// 模块位置：设备层；通过统一端口隔离相机和PLC供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "devices/camera/vendor/hikvision_camera_device.h"

#include "MvCameraControl.h"

#include <QDateTime>

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <new>
#include <vector>

namespace {

// 函数说明：settingRange 函数更新或应用对应的配置和状态。
CameraSettingRange settingRange(const MVCC_FLOATVALUE &value)
{
    CameraSettingRange range;
    range.minimum = value.fMin;
    range.maximum = value.fMax;
    range.current = value.fCurValue;
    return range;
}

// 函数说明：inRange 函数实现名称所表示的处理步骤。
bool inRange(float value, const MVCC_FLOATVALUE &range)
{
    return value >= range.fMin && value <= range.fMax;
}

} // namespace

// 组件说明：HikvisionCameraDevice 组件提供对应硬件设备能力的统一实现。
struct HikvisionCameraDevice::Impl
{
    // 函数说明：Impl 构造函数创建组件并初始化其依赖和初始状态。
    Impl()
    {
        std::memset(&devices, 0, sizeof(devices));
    }

    // 函数说明：onImage 函数执行对应事件或业务处理。
    static void __stdcall onImage(
        unsigned char *data,
        MV_FRAME_OUT_INFO_EX *info,
        void *user)
    {
        Impl *self = static_cast<Impl *>(user);
        if (!self || !data || !info) {
            return;
        }

        try {
        const bool mono =
                info->enPixelType == PixelType_Gvsp_Mono8
                || info->enPixelType == PixelType_Gvsp_Mono10
                || info->enPixelType == PixelType_Gvsp_Mono10_Packed
                || info->enPixelType == PixelType_Gvsp_Mono12
                || info->enPixelType == PixelType_Gvsp_Mono12_Packed;
        cv::Mat image;
        if (mono) {
            image = cv::Mat(
                        static_cast<int>(info->nHeight),
                        static_cast<int>(info->nWidth),
                        CV_8UC1,
                        data).clone();
        } else {
            const unsigned int bufferSize =
                    info->nWidth * info->nHeight * 3U + 2048U;
            std::vector<unsigned char> converted(bufferSize);
            MV_CC_PIXEL_CONVERT_PARAM parameters;
            std::memset(&parameters, 0, sizeof(parameters));
            parameters.nWidth = info->nWidth;
            parameters.nHeight = info->nHeight;
            parameters.pSrcData = data;
            parameters.nSrcDataLen = info->nFrameLen;
            parameters.enSrcPixelType = info->enPixelType;
            parameters.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
            parameters.pDstBuffer = converted.data();
            parameters.nDstBufferSize = bufferSize;
            if (!self->handle
                    || MV_CC_ConvertPixelType(
                        self->handle, &parameters) != MV_OK) {
                std::lock_guard<std::mutex> lock(self->mutex);
                self->callbackError = MV_E_PARAMETER;
                self->condition.notify_all();
                return;
            }
            image = cv::Mat(
                        static_cast<int>(info->nHeight),
                        static_cast<int>(info->nWidth),
                        CV_8UC3,
                        converted.data()).clone();
        }
        if (image.empty()) {
            return;
        }

        CameraFrame frame;
        frame.timestampUtc = QDateTime::currentDateTimeUtc();
        frame.image = image;

        std::lock_guard<std::mutex> lock(self->mutex);
        frame.sequence = ++self->sequence;
        if (self->frames.size() >= 2U) {
            self->callbackError = MV_E_BUFOVER;
            self->condition.notify_all();
            return;
        }
        self->frames.push_back(frame);
        self->condition.notify_one();
        } catch (const cv::Exception &) {
            std::lock_guard<std::mutex> lock(self->mutex);
            self->callbackError = MV_E_RESOURCE;
            self->condition.notify_all();
        } catch (const std::bad_alloc &) {
            std::lock_guard<std::mutex> lock(self->mutex);
            self->callbackError = MV_E_RESOURCE;
            self->condition.notify_all();
        }
    }

    MV_CC_DEVICE_INFO_LIST devices;
    void *handle = nullptr;
    bool grabbing = false;
    bool interrupted = false;
    int callbackError = MV_OK;
    std::uint64_t sequence = 0;
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<CameraFrame> frames;
};

// 函数说明：HikvisionCameraDevice 构造函数创建组件并初始化其依赖和初始状态。
HikvisionCameraDevice::HikvisionCameraDevice()
    : m_impl(new Impl)
{
}

// 函数说明：~HikvisionCameraDevice 析构函数按生命周期要求释放组件持有的资源。
HikvisionCameraDevice::~HikvisionCameraDevice()
{
    close();
}

// 函数说明：enumerate 函数创建、准备或启动对应流程。
CameraResult HikvisionCameraDevice::enumerate(int *deviceCount)
{
    std::memset(&m_impl->devices, 0, sizeof(m_impl->devices));
    const int result = MV_CC_EnumDevices(
                MV_GIGE_DEVICE | MV_USB_DEVICE,
                &m_impl->devices);
    if (deviceCount) {
        *deviceCount = result == MV_OK
                ? static_cast<int>(m_impl->devices.nDeviceNum)
                : 0;
    }
    return result == MV_OK
            ? CameraResult()
            : CameraResult::deviceError(result);
}

// 函数说明：openFirst 函数创建、准备或启动对应流程。
CameraResult HikvisionCameraDevice::openFirst()
{
    if (m_impl->handle) {
        CameraResult result;
        result.code = CameraResultCode::InvalidState;
        result.nativeErrorCode = MV_E_CALLORDER;
        return result;
    }
    if (m_impl->devices.nDeviceNum == 0U) {
        int count = 0;
        const CameraResult enumeration = enumerate(&count);
        if (!enumeration.isSuccess() || count <= 0) {
            return enumeration.isSuccess()
                    ? CameraResult::deviceError(MV_E_NODATA)
                    : enumeration;
        }
    }

    int nativeResult = MV_CC_CreateHandle(
                &m_impl->handle,
                m_impl->devices.pDeviceInfo[0]);
    if (nativeResult != MV_OK) {
        m_impl->handle = nullptr;
        return CameraResult::deviceError(nativeResult);
    }
    nativeResult = MV_CC_OpenDevice(m_impl->handle);
    if (nativeResult != MV_OK) {
        MV_CC_DestroyHandle(m_impl->handle);
        m_impl->handle = nullptr;
        return CameraResult::deviceError(nativeResult);
    }
    return CameraResult();
}

// 函数说明：applySettings 函数更新或应用对应的配置和状态。
CameraResult HikvisionCameraDevice::applySettings(
    const CameraSettings &settings)
{
    if (!m_impl->handle) {
        CameraResult result;
        result.code = CameraResultCode::InvalidState;
        result.nativeErrorCode = MV_E_HANDLE;
        return result;
    }

    MVCC_FLOATVALUE exposure;
    MVCC_FLOATVALUE gain;
    std::memset(&exposure, 0, sizeof(exposure));
    std::memset(&gain, 0, sizeof(gain));
    int nativeResult = MV_CC_GetFloatValue(
                m_impl->handle, "ExposureTime", &exposure);
    if (nativeResult != MV_OK) {
        return CameraResult::deviceError(nativeResult);
    }
    nativeResult = MV_CC_GetFloatValue(
                m_impl->handle, "Gain", &gain);
    if (nativeResult != MV_OK) {
        return CameraResult::deviceError(nativeResult);
    }

    CameraResult result;
    result.exposureRange = settingRange(exposure);
    result.gainRange = settingRange(gain);
    if ((settings.updateExposure
         && !inRange(settings.exposure, exposure))
            || (settings.updateGain
                && !inRange(settings.gain, gain))) {
        result.code = CameraResultCode::InvalidSettings;
        return result;
    }
    if (settings.updateExposure) {
        nativeResult = MV_CC_SetFloatValue(
                    m_impl->handle,
                    "ExposureTime",
                    settings.exposure);
        if (nativeResult != MV_OK) {
            return CameraResult::deviceError(nativeResult);
        }
        result.exposureRange.current = settings.exposure;
    }
    if (settings.updateGain) {
        nativeResult = MV_CC_SetFloatValue(
                    m_impl->handle, "Gain", settings.gain);
        if (nativeResult != MV_OK) {
            return CameraResult::deviceError(nativeResult);
        }
        result.gainRange.current = settings.gain;
    }
    if (settings.updateTriggerDelay) {
        nativeResult = MV_CC_SetFloatValue(
                    m_impl->handle,
                    "TriggerDelay",
                    settings.triggerDelayMicroseconds);
        if (nativeResult != MV_OK) {
            return CameraResult::deviceError(nativeResult);
        }
    }
    if (settings.updateLineDebouncerTime) {
        nativeResult = MV_CC_SetEnumValue(
                    m_impl->handle,
                    "LineDebouncerTime",
                    settings.lineDebouncerTime);
        if (nativeResult != MV_OK) {
            return CameraResult::deviceError(nativeResult);
        }
    }
    return result;
}

// 函数说明：setTriggerMode 函数更新或应用对应的配置和状态。
CameraResult HikvisionCameraDevice::setTriggerMode(
    CameraTriggerMode mode)
{
    if (!m_impl->handle) {
        CameraResult result;
        result.code = CameraResultCode::InvalidState;
        result.nativeErrorCode = MV_E_HANDLE;
        return result;
    }
    int nativeResult = MV_CC_SetEnumValue(
                m_impl->handle, "TriggerMode", 1U);
    if (nativeResult == MV_OK) {
        nativeResult = MV_CC_SetEnumValue(
                    m_impl->handle,
                    "TriggerSource",
                    mode == CameraTriggerMode::Software ? 7U : 0U);
    }
    if (nativeResult != MV_OK) {
        return CameraResult::deviceError(nativeResult);
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->frames.clear();
    m_impl->callbackError = MV_OK;
    m_impl->interrupted = false;
    return CameraResult();
}

// 函数说明：startGrabbing 函数创建、准备或启动对应流程。
CameraResult HikvisionCameraDevice::startGrabbing()
{
    if (!m_impl->handle) {
        CameraResult result;
        result.code = CameraResultCode::InvalidState;
        result.nativeErrorCode = MV_E_HANDLE;
        return result;
    }
    if (m_impl->grabbing) {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->interrupted = false;
        m_impl->callbackError = MV_OK;
        return CameraResult();
    }
    int nativeResult = MV_CC_RegisterImageCallBackEx(
                m_impl->handle,
                &Impl::onImage,
                m_impl.get());
    if (nativeResult == MV_OK) {
        nativeResult = MV_CC_StartGrabbing(m_impl->handle);
    }
    if (nativeResult != MV_OK) {
        return CameraResult::deviceError(nativeResult);
    }
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->frames.clear();
        m_impl->interrupted = false;
        m_impl->callbackError = MV_OK;
    }
    m_impl->grabbing = true;
    return CameraResult();
}

// 函数说明：triggerSoftware 函数执行对应事件或业务处理。
CameraResult HikvisionCameraDevice::triggerSoftware()
{
    if (!m_impl->handle || !m_impl->grabbing) {
        CameraResult result;
        result.code = CameraResultCode::InvalidState;
        result.nativeErrorCode = MV_E_CALLORDER;
        return result;
    }
    const int nativeResult = MV_CC_SetCommandValue(
                m_impl->handle, "TriggerSoftware");
    return nativeResult == MV_OK
            ? CameraResult()
            : CameraResult::deviceError(nativeResult);
}

// 函数说明：waitNextFrame 函数读取、等待或计算对应的数据。
CameraFrameResult HikvisionCameraDevice::waitNextFrame(int timeoutMs)
{
    CameraFrameResult result;
    if (!m_impl->handle || !m_impl->grabbing) {
        result.status = CameraFrameStatus::DeviceError;
        result.nativeErrorCode = MV_E_CALLORDER;
        return result;
    }

    std::unique_lock<std::mutex> lock(m_impl->mutex);
    const bool ready = m_impl->condition.wait_for(
                lock,
                std::chrono::milliseconds(timeoutMs),
                [this]() {
        return m_impl->interrupted
                || m_impl->callbackError != MV_OK
                || !m_impl->frames.empty();
    });
    if (!ready) {
        result.status = CameraFrameStatus::Timeout;
        return result;
    }
    if (m_impl->interrupted) {
        result.status = CameraFrameStatus::Interrupted;
        return result;
    }
    if (m_impl->callbackError != MV_OK) {
        result.status = CameraFrameStatus::DeviceError;
        result.nativeErrorCode = m_impl->callbackError;
        m_impl->callbackError = MV_OK;
        return result;
    }
    result.status = CameraFrameStatus::FrameReady;
    result.frame = m_impl->frames.front();
    m_impl->frames.pop_front();
    return result;
}

// 函数说明：interruptWait 函数停止流程、清理状态或释放对应资源。
void HikvisionCameraDevice::interruptWait()
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->interrupted = true;
    m_impl->condition.notify_all();
}

// 函数说明：stopGrabbing 函数停止流程、清理状态或释放对应资源。
CameraResult HikvisionCameraDevice::stopGrabbing()
{
    interruptWait();
    if (!m_impl->handle || !m_impl->grabbing) {
        return CameraResult();
    }
    const int nativeResult = MV_CC_StopGrabbing(m_impl->handle);
    if (nativeResult == MV_OK) {
        m_impl->grabbing = false;
        return CameraResult();
    }
    return CameraResult::deviceError(nativeResult);
}

// 函数说明：close 函数停止流程、清理状态或释放对应资源。
CameraResult HikvisionCameraDevice::close()
{
    interruptWait();
    if (!m_impl->handle) {
        return CameraResult();
    }
    CameraResult stop = stopGrabbing();
    const int closeResult = MV_CC_CloseDevice(m_impl->handle);
    const int destroyResult = MV_CC_DestroyHandle(m_impl->handle);
    m_impl->handle = nullptr;
    m_impl->grabbing = false;
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->frames.clear();
    }
    if (!stop.isSuccess()) {
        return stop;
    }
    if (closeResult != MV_OK) {
        return CameraResult::deviceError(closeResult);
    }
    return destroyResult == MV_OK
            ? CameraResult()
            : CameraResult::deviceError(destroyResult);
}
