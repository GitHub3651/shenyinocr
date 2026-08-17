#include "runtime/camera_session.h"

#include "runtime/inspection_runtime.h"

#include <QDebug>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {

QString cameraErrorText(const QString &operation, int nativeErrorCode)
{
    return QStringLiteral("%1，错误码：%2")
            .arg(operation)
            .arg(nativeErrorCode);
}

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

} // namespace

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

CameraSession::~CameraSession()
{
    close();
}

void CameraSession::setCallbacks(
    const CameraSessionCallbacks &callbacks)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks = callbacks;
}

bool CameraSession::isOpen() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_open;
}

bool CameraSession::isCapturing() const
{
    return m_captureWorker.isRunning();
}

InspectionCameraOpenResult CameraSession::openFirst(
    int savedExposure,
    const PersistAdjustedExposure &persistAdjustedExposure)
{
    InspectionCameraOpenResult output;
    CameraResult result = m_cameraDevice->enumerate(&output.deviceCount);
    if (!result.isSuccess() || output.deviceCount <= 0) {
        output.issue = InspectionCameraOpenIssue::DeviceNotFound;
        output.diagnostic = QString::number(result.nativeErrorCode);
        return output;
    }
    result = m_cameraDevice->openFirst();
    if (!result.isSuccess()) {
        output.issue = InspectionCameraOpenIssue::DeviceOpenFailed;
        output.diagnostic = QString::number(result.nativeErrorCode);
        return output;
    }
    result = m_cameraDevice->setTriggerMode(CameraTriggerMode::Software);
    if (!result.isSuccess()) {
        m_cameraDevice->close();
        output.issue = InspectionCameraOpenIssue::InitializationFailed;
        output.diagnostic = cameraErrorText(
                    QStringLiteral("相机切换软件触发失败"),
                    result.nativeErrorCode);
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
        return output;
    }
    result = m_cameraDevice->startGrabbing();
    if (!result.isSuccess()) {
        m_cameraDevice->close();
        output.issue = InspectionCameraOpenIssue::InitializationFailed;
        output.diagnostic = cameraErrorText(
                    QStringLiteral("相机启动抓图失败"),
                    result.nativeErrorCode);
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
    return output;
}

void CameraSession::close()
{
    m_intentionalStop = true;
    m_captureWorker.stop();
    m_cameraDevice->close();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_open = false;
    m_prepared = false;
    m_preview = false;
    m_currentImage.release();
}

InspectionCameraParameterResult CameraSession::queryExposureRange()
{
    return parameterResult(
                m_cameraDevice->applySettings(CameraSettings()),
                true);
}

InspectionCameraParameterResult CameraSession::queryGainRange()
{
    return parameterResult(
                m_cameraDevice->applySettings(CameraSettings()),
                false);
}

InspectionCameraParameterResult CameraSession::applyExposure(int exposure)
{
    CameraSettings settings;
    settings.updateExposure = true;
    settings.exposure = static_cast<float>(exposure);
    return parameterResult(
                m_cameraDevice->applySettings(settings),
                true);
}

InspectionCameraParameterResult CameraSession::applyGain(int gain)
{
    CameraSettings settings;
    settings.updateGain = true;
    settings.gain = static_cast<float>(gain);
    return parameterResult(
                m_cameraDevice->applySettings(settings),
                false);
}

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
    if (!m_positioner.configure(
                configuration.runPlan.trackingKind,
                configuration.trackingProfiles,
                configuration.singleDatePolygon,
                configuration.singleTrackingTemplate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("运行定位资源初始化失败。");
        }
        return false;
    }

    CameraResult result;
    if (configuration.runPlan.acquisitionKind
            == InspectionAcquisitionKind::HardwareTrigger) {
        result = m_cameraDevice->stopGrabbing();
        if (result.isSuccess()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
    if (result.isSuccess()) {
        result = m_cameraDevice->setTriggerMode(
                    configuration.runPlan.acquisitionKind
                    == InspectionAcquisitionKind::HardwareTrigger
                    ? CameraTriggerMode::HardwareLine0
                    : CameraTriggerMode::Software);
    }
    CameraSettings settings;
    settings.updateExposure = true;
    settings.exposure = static_cast<float>(configuration.exposure);
    settings.updateGain = true;
    settings.gain = static_cast<float>(configuration.gain);
    settings.updateTriggerDelay =
            configuration.runPlan.acquisitionKind
            == InspectionAcquisitionKind::HardwareTrigger;
    if (settings.updateTriggerDelay) {
        settings.triggerDelayMicroseconds =
                configuration.hardwareTriggerDelayMicroseconds;
    }
    if (result.isSuccess()) {
        result = m_cameraDevice->applySettings(settings);
    }
    if (result.isSuccess()
            && configuration.runPlan.acquisitionKind
               == InspectionAcquisitionKind::HardwareTrigger) {
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
    const CaptureMode mode = m_configuration.runPlan.acquisitionKind
            == InspectionAcquisitionKind::HardwareTrigger
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

bool CameraSession::startPreview(
    quint64 sessionId,
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
    m_previewSessionId = sessionId;
    m_previewFramePending.store(false);
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

void CameraSession::acknowledgePreviewFrame(quint64 sessionId)
{
    if (sessionId == m_previewSessionId) {
        m_previewFramePending.store(false);
    }
}

bool CameraSession::stopPreview()
{
    m_intentionalStop = true;
    m_captureWorker.stop();
    m_preview = false;
    m_previewFramePending.store(false);
    return !m_captureWorker.isRunning();
}

bool CameraSession::hasCurrentImage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_currentImage.empty();
}

cv::Mat CameraSession::currentImageClone() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentImage.clone();
}

void CameraSession::replaceCurrentImage(const cv::Mat &image)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentImage = image.clone();
}

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

void CameraSession::handleFrame(const CameraFrame &frame)
{
    cv::Mat image;
    if (!FramePreprocessor::transform(
                frame.image,
                m_configuration.framePreprocess,
                &image)) {
        return;
    }
    replaceCurrentImage(image);
    const CameraSessionCallbacks callbacks = callbacksSnapshot();
    if (m_preview) {
        if (!m_previewFramePending.exchange(true)
                && callbacks.previewFrameReady) {
            callbacks.previewFrameReady(m_previewSessionId, image);
        }
        return;
    }

    DetectionPose pose;
    if (m_configuration.runPlan.trackingKind
            != InspectionTrackingKind::WholeFrame) {
        pose = m_positioner.locate(image);
        if (callbacks.trackingPoseReady) {
            callbacks.trackingPoseReady(pose);
        }
    }
    if (callbacks.streamingFrameReady) {
        callbacks.streamingFrameReady(image);
    }
    submitFrame(image, pose);
}

void CameraSession::handleCaptureError(
    CameraFrameStatus status,
    int nativeErrorCode)
{
    const CameraSessionCallbacks callbacks = callbacksSnapshot();
    if (m_preview) {
        if (callbacks.previewFailed) {
            const QString reason = status == CameraFrameStatus::Timeout
                    ? QStringLiteral(
                        "连续3次等待相机图像超时，请检查相机连接和触发设置。")
                    : cameraErrorText(
                        QStringLiteral("模板实时取景失败"),
                        nativeErrorCode);
            callbacks.previewFailed(m_previewSessionId, reason);
        }
        return;
    }
    qWarning() << "[CAMERA_CAPTURE] formal capture stopped:"
               << static_cast<int>(status)
               << nativeErrorCode;
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

void CameraSession::handleCaptureStopped()
{
    const bool preview = m_preview;
    m_preview = false;
    if (!m_intentionalStop) {
        const CameraSessionCallbacks callbacks = callbacksSnapshot();
        if (callbacks.captureStopped) {
            callbacks.captureStopped(preview);
        }
    }
}

void CameraSession::submitFrame(
    const cv::Mat &image,
    const DetectionPose &pose)
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
    const bool wholeFrame = m_configuration.runPlan.trackingKind
            == InspectionTrackingKind::WholeFrame;
    const bool profileMode = m_configuration.runPlan.trackingKind
            == InspectionTrackingKind::WordProfiles;
    if (!wholeFrame && !profileMode && !pose.valid) {
        return;
    }
    const std::shared_ptr<const FrameData> frame =
            m_runtime->acceptFrame(image);
    if (!frame) {
        return;
    }

    if (m_configuration.runPlan.acquisitionKind
            == InspectionAcquisitionKind::SoftwareTrigger) {
        const bool accepted = wholeFrame
                ? m_runtime->submitDetectionFrame(frame)
                : m_runtime->submitDetectionWorkItem(
                    makeDetectionWorkItem(frame, pose));
        if (!accepted) {
            qDebug() << "[DETECTION_WORKER] software frame rejected"
                     << frame->productKey.runId
                     << frame->productKey.sequence;
        }
        return;
    }

    const DetectionWorkSubmissionResult submission = wholeFrame
            ? m_runtime->trySubmitDetectionFrame(frame)
            : m_runtime->trySubmitDetectionWorkItem(
                makeDetectionWorkItem(frame, pose));
    if (submission == DetectionWorkSubmissionResult::QueueFull) {
        const CameraSessionCallbacks callbacks = callbacksSnapshot();
        if (callbacks.enterFault) {
            callbacks.enterFault(
                        InspectionFaultReason::HardTriggerQueueOverflow,
                        QStringLiteral(
                            "硬触发 FIFO 无法接收产品 %1/%2，队列容量 %3。")
                        .arg(frame->productKey.runId)
                        .arg(frame->productKey.sequence)
                        .arg(static_cast<qulonglong>(
                            m_runtime
                            ->detectionWorkerQueueCapacity())));
        }
    }
}

CameraSessionCallbacks CameraSession::callbacksSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_callbacks;
}
