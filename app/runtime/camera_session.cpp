// 文件作用：本文件用于管理相机打开、参数下发、预览、正式采集和停止恢复的完整会话。
// 主要职责：管理相机打开、参数下发、预览、正式采集和停止恢复的完整会话。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/camera_session.h"

#include "runtime/inspection_runtime.h"
#include "system_support/logging/log_categories.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {

// 函数说明：cameraErrorText 函数实现名称所表示的处理步骤。
QString cameraErrorText(const QString &, int)
{
    return QStringLiteral("相机异常。");
}

// 函数说明：integerRange 函数实现名称所表示的处理步骤。
bool integerRange(
    const CameraSettingRange &range,
    int *minimum,
    int *maximum)
{
    if (!minimum || !maximum
            || !std::isfinite(range.minimum)
            || !std::isfinite(range.maximum)) {
        return false;
    }
    const double roundedMinimum = std::ceil(range.minimum);
    const double roundedMaximum = std::floor(range.maximum);
    if (roundedMinimum > roundedMaximum
            || roundedMinimum
               < static_cast<double>((std::numeric_limits<int>::min)())
            || roundedMaximum
               > static_cast<double>((std::numeric_limits<int>::max)())) {
        return false;
    }
    *minimum = static_cast<int>(roundedMinimum);
    *maximum = static_cast<int>(roundedMaximum);
    return true;
}

QString cameraFrameStatusName(CameraFrameStatus status)
{
    switch (status) {
    case CameraFrameStatus::FrameReady:
        return QStringLiteral("frame_ready");
    case CameraFrameStatus::Timeout:
        return QStringLiteral("timeout");
    case CameraFrameStatus::Interrupted:
        return QStringLiteral("interrupted");
    case CameraFrameStatus::DeviceError:
        return QStringLiteral("device_error");
    }
    return QStringLiteral("unknown");
}

} // namespace

// 函数说明：CameraSession 构造函数创建组件并初始化其依赖和初始状态。
CameraSession::CameraSession(
    const std::shared_ptr<ICameraDevice> &cameraDevice,
    InspectionRuntime *runtime)
    : m_cameraDevice(cameraDevice),
      m_runtime(runtime),
      m_captureWorker(cameraDevice)
{
    if (!m_runtime) {
        throw std::invalid_argument(
                    "CameraSession requires InspectionRuntime");
    }
}

// 函数说明：~CameraSession 析构函数按生命周期要求释放组件持有的资源。
CameraSession::~CameraSession()
{
    close();
}

// 函数说明：setCallbacks 函数更新或应用对应的配置和状态。
void CameraSession::setCallbacks(
    const CameraSessionCallbacks &callbacks)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks = callbacks;
}

// 函数说明：isOpen 函数检查相关状态并返回判断结果。
bool CameraSession::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_open;
}

// 函数说明：isCapturing 函数检查相关状态并返回判断结果。
bool CameraSession::isCapturing() const
{
    return m_captureWorker.isRunning();
}

// 函数说明：openFirst 函数创建、准备或启动对应流程。
InspectionCameraOpenResult CameraSession::openFirst(
    int savedExposure,
    const PersistAdjustedExposure &persistAdjustedExposure)
{
    InspectionCameraOpenResult output;
    CameraResult result = m_cameraDevice->enumerate(&output.deviceCount);
    if (!result.isSuccess()) {
        output.issue = InspectionCameraOpenIssue::DeviceError;
        output.diagnostic = QStringLiteral("相机异常。");
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.enumerate_failed nativeCode=%1")
                   .arg(result.nativeErrorCode);
        return output;
    }
    if (output.deviceCount <= 0) {
        output.issue = InspectionCameraOpenIssue::DeviceNotFound;
        qCWarning(logDevice).noquote() << "event=camera.not_found count=0";
        return output;
    }
    result = m_cameraDevice->openFirst();
    if (!result.isSuccess()) {
        output.issue = InspectionCameraOpenIssue::DeviceOpenFailed;
        output.diagnostic = QStringLiteral("相机异常。");
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.open_failed nativeCode=%1")
                   .arg(result.nativeErrorCode);
        return output;
    }
    result = m_cameraDevice->setTriggerMode(CameraTriggerMode::Software);
    if (!result.isSuccess()) {
        m_cameraDevice->close();
        output.issue = InspectionCameraOpenIssue::InitializationFailed;
        output.diagnostic = cameraErrorText(
                    QStringLiteral("相机切换软件触发失败"),
                    result.nativeErrorCode);
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.initialize_failed step=trigger_mode nativeCode=%1")
                   .arg(result.nativeErrorCode);
        return output;
    }

    QString adjustmentMessage;
    const InspectionCameraParameterResult exposure = applySavedExposure(
                savedExposure,
                persistAdjustedExposure,
                &adjustmentMessage);
    if (!exposure.success) {
        m_cameraDevice->close();
        output.issue = InspectionCameraOpenIssue::ExposureFailed;
        output.diagnostic = exposure.diagnostic;
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.initialize_failed step=exposure nativeCode=%1 reason=%2")
                   .arg(exposure.nativeErrorCode)
                   .arg(exposure.diagnostic);
        return output;
    }
    CameraSettings openTiming;
    openTiming.updateTriggerDelay = true;
    result = m_cameraDevice->applySettings(openTiming);
    if (!result.isSuccess()) {
        m_cameraDevice->close();
        output.issue = InspectionCameraOpenIssue::InitializationFailed;
        output.diagnostic = cameraErrorText(
                    QStringLiteral("相机触发延时初始化失败"),
                    result.nativeErrorCode);
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.initialize_failed step=trigger_delay nativeCode=%1")
                   .arg(result.nativeErrorCode);
        return output;
    }
    result = m_cameraDevice->startGrabbing();
    if (!result.isSuccess()) {
        m_cameraDevice->close();
        output.issue = InspectionCameraOpenIssue::InitializationFailed;
        output.diagnostic = cameraErrorText(
                    QStringLiteral("相机启动抓图失败"),
                    result.nativeErrorCode);
        qCCritical(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.initialize_failed step=start_grabbing nativeCode=%1")
                   .arg(result.nativeErrorCode);
        return output;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = true;
    }
    output.appliedExposure = static_cast<int>(exposure.actualValue);
    output.exposureMinimum = exposure.minimumValue;
    output.exposureMaximum = exposure.maximumValue;
    output.exposureAdjusted = output.appliedExposure != savedExposure;
    output.adjustmentMessage = adjustmentMessage;
    QString summary = QStringLiteral(
                "event=camera.opened devices=%1 exposure=%2")
            .arg(output.deviceCount)
            .arg(output.appliedExposure);
    if (output.exposureAdjusted) {
        summary += QStringLiteral(
                    " requestedExposure=%1 exposureRange=%2-%3")
                .arg(savedExposure)
                .arg(output.exposureMinimum)
                .arg(output.exposureMaximum);
    }
    qCInfo(logDevice).noquote() << summary;
    return output;
}

// 函数说明：close 函数停止流程、清理状态或释放对应资源。
void CameraSession::close()
{
    const bool wasOpen = isOpen();
    m_intentionalStop = true;
    m_captureWorker.stop();
    const CameraResult closed = m_cameraDevice->close();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = false;
        m_prepared = false;
        m_preview = false;
        m_currentImage.release();
    }
    if (closed.isSuccess() && wasOpen) {
        qCInfo(logDevice).noquote() << "event=camera.closed";
    } else if (!closed.isSuccess()) {
        qCWarning(logDevice).noquote()
                << QStringLiteral("event=camera.close_failed nativeCode=%1")
                   .arg(closed.nativeErrorCode);
    }
}

// 函数说明：queryExposureRange 函数读取、等待或计算对应的数据。
InspectionCameraParameterResult CameraSession::queryExposureRange()
{
    return parameterResult(
                m_cameraDevice->applySettings(CameraSettings()),
                true);
}

// 函数说明：queryGainRange 函数读取、等待或计算对应的数据。
InspectionCameraParameterResult CameraSession::queryGainRange()
{
    return parameterResult(
                m_cameraDevice->applySettings(CameraSettings()),
                false);
}

// 函数说明：applyExposure 函数更新或应用对应的配置和状态。
InspectionCameraParameterResult CameraSession::applyExposure(int exposure)
{
    CameraSettings settings;
    settings.updateExposure = true;
    settings.exposure = static_cast<float>(exposure);
    return parameterResult(
                m_cameraDevice->applySettings(settings),
                true);
}

// 函数说明：applyGain 函数更新或应用对应的配置和状态。
InspectionCameraParameterResult CameraSession::applyGain(int gain)
{
    CameraSettings settings;
    settings.updateGain = true;
    settings.gain = static_cast<float>(gain);
    return parameterResult(
                m_cameraDevice->applySettings(settings),
                false);
}

// 函数说明：prepareInspection 函数创建、准备或启动对应流程。
bool CameraSession::prepareInspection(
    const CameraSessionCaptureConfiguration &configuration,
    QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!isOpen() || m_captureWorker.isRunning()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("相机未打开或采集仍在运行。");
        }
        return false;
    }
    CameraResult result;
    if (configuration.hardwareTriggerEnabled) {
        result = m_cameraDevice->stopGrabbing();
        if (result.isSuccess()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    if (result.isSuccess()) {
        result = m_cameraDevice->setTriggerMode(
                    configuration.hardwareTriggerEnabled
                    ? CameraTriggerMode::HardwareLine0
                    : CameraTriggerMode::Software);
    }
    CameraSettings settings;
    settings.updateExposure = true;
    settings.exposure = static_cast<float>(configuration.exposure);
    settings.updateGain = true;
    settings.gain = static_cast<float>(configuration.gain);
    settings.updateTriggerDelay = configuration.hardwareTriggerEnabled;
    if (settings.updateTriggerDelay) {
        settings.triggerDelayMicroseconds =
                configuration.hardwareTriggerDelayMicroseconds;
    }
    if (result.isSuccess()) {
        result = m_cameraDevice->applySettings(settings);
    }
    if (result.isSuccess() && configuration.hardwareTriggerEnabled) {
        result = m_cameraDevice->startGrabbing();
        if (result.isSuccess()) {
            CameraSettings debounce;
            debounce.updateLineDebouncerTime = true;
            result = m_cameraDevice->applySettings(debounce);
        }
        if (result.isSuccess()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    if (!result.isSuccess()) {
        if (errorMessage) {
            *errorMessage = cameraErrorText(
                        QStringLiteral("相机运行参数应用失败"),
                        result.nativeErrorCode);
        }
        return false;
    }
    m_configuration = configuration;
    m_prepared = true;
    m_preview = false;
    return true;
}

// 函数说明：startInspection 函数创建、准备或启动对应流程。
bool CameraSession::startInspection(QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!m_prepared || !isOpen() || m_captureWorker.isRunning()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("相机采集会话尚未准备完成。");
        }
        return false;
    }
    m_intentionalStop = false;
    CaptureWorkerCallbacks callbacks;
    callbacks.frameReady = [this](const CameraFrame &frame) {
        handleFrame(frame);
    };
    callbacks.captureError = [this](
            CameraFrameStatus status,
            int nativeErrorCode) {
        handleCaptureError(status, nativeErrorCode);
    };
    callbacks.stopped = [this]() { handleCaptureStopped(); };
    const CaptureMode mode = m_configuration.hardwareTriggerEnabled
            ? CaptureMode::HardwareTrigger
            : CaptureMode::SoftwareTrigger;
    if (!m_captureWorker.start(mode, callbacks)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("采集线程启动失败。");
        }
        return false;
    }
    return true;
}

// 函数说明：stopInspection 函数停止流程、清理状态或释放对应资源。
CameraCaptureStopResult CameraSession::stopInspection()
{
    CameraCaptureStopResult result;
    result.wasRunning = m_captureWorker.isRunning();
    m_intentionalStop = true;
    m_captureWorker.stop();
    result.stopped = !m_captureWorker.isRunning();
    m_prepared = false;
    m_preview = false;
    return result;
}

// 函数说明：restorePreviewReady 函数校验、转换或恢复对应数据。
InspectionCameraRecoveryResult CameraSession::restorePreviewReady(
    int savedExposure,
    const PersistAdjustedExposure &persistAdjustedExposure)
{
    InspectionCameraRecoveryResult output;
    output.recoveryAttempted = true;
    output.cameraOpen = isOpen();
    if (!output.cameraOpen) {
        output.issue = InspectionCameraRecoveryIssue::MissingCamera;
        return output;
    }
    CameraResult result = m_cameraDevice->close();
    if (result.isSuccess()) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_open = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        result = m_cameraDevice->openFirst();
    }
    if (result.isSuccess()) {
        result = m_cameraDevice->setTriggerMode(CameraTriggerMode::Software);
    }
    const InspectionCameraParameterResult exposure = result.isSuccess()
            ? applySavedExposure(
                savedExposure,
                persistAdjustedExposure,
                &output.adjustmentMessage)
            : InspectionCameraParameterResult();
    if (result.isSuccess() && exposure.success) {
        CameraSettings timing;
        timing.updateTriggerDelay = true;
        result = m_cameraDevice->applySettings(timing);
    }
    if (result.isSuccess() && exposure.success) {
        result = m_cameraDevice->startGrabbing();
    }
    if (!result.isSuccess()) {
        output.issue = InspectionCameraRecoveryIssue::InitializationFailed;
        output.errorMessage = cameraErrorText(
                    QStringLiteral("相机恢复失败"),
                    result.nativeErrorCode);
        m_cameraDevice->close();
        output.cameraOpen = false;
    } else if (!exposure.success) {
        output.issue = InspectionCameraRecoveryIssue::ExposureRejected;
        output.errorMessage = exposure.diagnostic;
        m_cameraDevice->close();
        output.cameraOpen = false;
    } else {
        output.cameraOpen = true;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_open = output.cameraOpen;
    }
    m_prepared = false;
    m_preview = false;
    return output;
}

// 函数说明：startPreview 函数创建、准备或启动对应流程。
bool CameraSession::startPreview(
    const FramePreprocessSettings &settings,
    QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!isOpen() || m_captureWorker.isRunning()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("相机未打开或正式检测仍在运行。");
        }
        return false;
    }
    const CameraResult mode =
            m_cameraDevice->setTriggerMode(CameraTriggerMode::Software);
    if (!mode.isSuccess()) {
        if (errorMessage) {
            *errorMessage = cameraErrorText(
                        QStringLiteral("相机切换软件触发失败"),
                        mode.nativeErrorCode);
        }
        return false;
    }
    m_configuration.framePreprocess = settings;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentImage.release();
    }
    m_preview = true;
    m_prepared = false;
    m_intentionalStop = false;
    CaptureWorkerCallbacks callbacks;
    callbacks.frameReady = [this](const CameraFrame &frame) {
        handleFrame(frame);
    };
    callbacks.captureError = [this](
            CameraFrameStatus status,
            int nativeErrorCode) {
        handleCaptureError(status, nativeErrorCode);
    };
    callbacks.stopped = [this]() { handleCaptureStopped(); };
    if (!m_captureWorker.start(CaptureMode::Preview, callbacks)) {
        m_preview = false;
        if (errorMessage) {
            *errorMessage = QStringLiteral("实时取景线程启动失败。");
        }
        return false;
    }
    return true;
}

// 函数说明：stopPreview 函数停止流程、清理状态或释放对应资源。
bool CameraSession::stopPreview()
{
    m_intentionalStop = true;
    m_captureWorker.stop();
    m_preview = false;
    return !m_captureWorker.isRunning();
}

// 函数说明：hasCurrentImage 函数检查相关状态并返回判断结果。
bool CameraSession::hasCurrentImage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_currentImage.empty();
}

// 函数说明：currentImageClone 函数读取、等待或计算对应的数据。
cv::Mat CameraSession::currentImageClone() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentImage.clone();
}

// 函数说明：parameterResult 函数实现名称所表示的处理步骤。
InspectionCameraParameterResult CameraSession::parameterResult(
    const CameraResult &result,
    bool exposure) const
{
    InspectionCameraParameterResult output;
    output.nativeErrorCode = result.nativeErrorCode;
    const CameraSettingRange &range = exposure
            ? result.exposureRange
            : result.gainRange;
    if (!integerRange(range, &output.minimumValue, &output.maximumValue)) {
        output.diagnostic = QStringLiteral(
                    "相机参数范围无效或相机尚未打开。");
        return output;
    }
    output.actualValue = range.current;
    if (!result.isSuccess()) {
        output.diagnostic = result.code == CameraResultCode::InvalidSettings
                ? QStringLiteral("输入值超出当前相机允许范围：%1 ~ %2")
                  .arg(output.minimumValue)
                  .arg(output.maximumValue)
                : cameraErrorText(
                    exposure
                    ? QStringLiteral("相机曝光设置失败")
                    : QStringLiteral("相机增益设置失败"),
                    result.nativeErrorCode);
        return output;
    }
    output.success = true;
    return output;
}

// 函数说明：applySavedExposure 函数更新或应用对应的配置和状态。
InspectionCameraParameterResult CameraSession::applySavedExposure(
    int savedExposure,
    const PersistAdjustedExposure &persistAdjustedExposure,
    QString *adjustmentMessage)
{
    const InspectionCameraParameterResult range = queryExposureRange();
    if (!range.success) {
        return range;
    }
    const int adjusted = std::max(
                range.minimumValue,
                std::min(savedExposure, range.maximumValue));
    InspectionCameraParameterResult output = applyExposure(adjusted);
    if (!output.success) {
        return output;
    }
    output.actualValue = adjusted;
    if (adjusted != savedExposure) {
        if (adjustmentMessage) {
            *adjustmentMessage = QString(
                        "原曝光值 %1 超出当前相机允许范围（%2 ~ %3），已调整为 %4。")
                    .arg(savedExposure)
                    .arg(output.minimumValue)
                    .arg(output.maximumValue)
                    .arg(adjusted);
        }
        QString persistError;
        if (!persistAdjustedExposure
                || !persistAdjustedExposure(adjusted, &persistError)) {
            output.success = false;
            output.diagnostic = persistError.isEmpty()
                    ? QStringLiteral("曝光已调整，但机器设置保存失败。")
                    : persistError;
        }
    }
    return output;
}

// 函数说明：handleFrame 函数执行对应事件或业务处理。
void CameraSession::handleFrame(const CameraFrame &frame)
{
    if (m_preview) {
        cv::Mat image;
        if (!FramePreprocessor::transform(
                    frame.image,
                    m_configuration.framePreprocess,
                    &image)) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_currentImage = image;
        }
        const CameraSessionCallbacks callbacks = callbacksSnapshot();
        if (callbacks.previewFrameReady) {
            callbacks.previewFrameReady(image);
        }
        return;
    }

    submitFrame(frame.image);
}

// 函数说明：handleCaptureError 函数执行对应事件或业务处理。
void CameraSession::handleCaptureError(
    CameraFrameStatus status,
    int nativeErrorCode)
{
    const CameraSessionCallbacks callbacks = callbacksSnapshot();
    if (m_preview) {
        const QString reason = status == CameraFrameStatus::Timeout
                ? QStringLiteral(
                    "连续3次等待相机图像超时，请检查相机连接和触发设置。")
                : cameraErrorText(
                    QStringLiteral("模板实时取景失败"),
                    nativeErrorCode);
        qCWarning(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.preview_failed status=%1 nativeCode=%2 reason=%3")
                   .arg(cameraFrameStatusName(status))
                   .arg(nativeErrorCode)
                   .arg(reason);
        if (callbacks.previewFailed) {
            callbacks.previewFailed(reason);
        }
        return;
    }
    qCCritical(logDevice).noquote()
            << QStringLiteral(
                "event=camera.capture_failed status=%1 nativeCode=%2")
               .arg(cameraFrameStatusName(status))
               .arg(nativeErrorCode);
    if (callbacks.enterFault) {
        callbacks.enterFault(
                    InspectionFaultReason::CameraDisconnected,
                    status == CameraFrameStatus::Timeout
                    ? QStringLiteral(
                        "正式检测连续等待相机图像超时，相机采集已停止。")
                    : cameraErrorText(
                        QStringLiteral("正式检测相机采集失败"),
                        nativeErrorCode));
    }
}

// 函数说明：handleCaptureStopped 函数执行对应事件或业务处理。
void CameraSession::handleCaptureStopped()
{
    const bool preview = m_preview;
    m_preview = false;
    if (!m_intentionalStop) {
        qCWarning(logDevice).noquote()
                << QStringLiteral(
                    "event=camera.capture_stopped_unexpected context=%1")
                   .arg(preview
                        ? QStringLiteral("preview")
                        : QStringLiteral("inspection"));
        const CameraSessionCallbacks callbacks = callbacksSnapshot();
        if (callbacks.captureStopped) {
            callbacks.captureStopped(preview);
        }
    }
}

// 函数说明：submitFrame 函数执行对应事件或业务处理。
void CameraSession::submitFrame(
    const cv::Mat &image)
{
    if (image.empty()) {
        return;
    }
    if (m_runtime->state() == InspectionRuntimeState::Fault) {
        m_runtime->acceptFrame(image);
        return;
    }
    if (!m_runtime->isDetectionWorkerActive()) {
        return;
    }
    const std::shared_ptr<const FrameData> frame =
            m_runtime->acceptFrame(image);
    if (!frame) {
        return;
    }
    if (!m_configuration.hardwareTriggerEnabled) {
        const bool accepted = m_runtime->submitDetectionFrame(frame);
        if (!accepted
                && m_runtime->state() == InspectionRuntimeState::Running) {
            qCWarning(logDetection).noquote()
                    << QStringLiteral(
                        "event=frame.rejected trigger=software reason=worker_unavailable");
        }
        return;
    }

    const DetectionWorkSubmissionResult submission =
            m_runtime->trySubmitDetectionFrame(frame);
    if (submission == DetectionWorkSubmissionResult::QueueFull) {
        qCCritical(logDetection).noquote()
                << QStringLiteral(
                    "event=frame.rejected trigger=hardware reason=queue_full capacity=%1")
                   .arg(static_cast<qulonglong>(
                            m_runtime->detectionWorkerQueueCapacity()));
        const CameraSessionCallbacks callbacks = callbacksSnapshot();
        if (callbacks.enterFault) {
            callbacks.enterFault(
                        InspectionFaultReason::HardTriggerQueueOverflow,
                        QStringLiteral(
                            "硬触发 FIFO 无法接收新图像，队列容量 %1。")
                        .arg(static_cast<qulonglong>(
                            m_runtime
                            ->detectionWorkerQueueCapacity())));
        }
    } else if (submission == DetectionWorkSubmissionResult::NotRunning
               && m_runtime->state() == InspectionRuntimeState::Running) {
        qCWarning(logDetection).noquote()
                << QStringLiteral(
                    "event=frame.rejected trigger=hardware reason=worker_unavailable");
    }
}

// 函数说明：callbacksSnapshot 函数实现名称所表示的处理步骤。
CameraSessionCallbacks CameraSession::callbacksSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_callbacks;
}
