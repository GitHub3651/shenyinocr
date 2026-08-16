#include "runtime/inspection_acquisition_controller.h"

#include "CameraThread.h"
#include "mythread.h"
#include "runtime/inspection_worker_configurator.h"

#include <QDebug>
#include <QMetaObject>

InspectionAcquisitionController::InspectionAcquisitionController(
    const std::shared_ptr<ICameraDevice> &cameraDevice,
    InspectionRuntimeController *runtimeController,
    const InspectionAcquisitionCallbacks &callbacks,
    QObject *parent)
    : QObject(parent),
      m_cameraDevice(cameraDevice),
      m_cameraOperations(cameraDevice.get()),
      m_runtimeController(runtimeController),
      m_callbacks(callbacks),
      m_imageBuffer(new cv::Mat)
{
    createSoftwareWorker();
}

InspectionAcquisitionController::~InspectionAcquisitionController()
{
    shutdown();
}

bool InspectionAcquisitionController::hasCamera() const
{
    return static_cast<bool>(m_cameraDevice);
}

bool InspectionAcquisitionController::isSoftwareRunning() const
{
    return m_softwareWorker && m_softwareWorker->isRunning();
}

bool InspectionAcquisitionController::isHardwareRunning() const
{
    return m_hardwareWorker && m_hardwareWorker->isRunning();
}

CameraOperationResult InspectionAcquisitionController::enumerateDevices(
    int *deviceCount)
{
    return m_cameraDevice
            ? m_cameraDevice->enumerateDevices(deviceCount)
            : CameraOperationResult(-1);
}

CameraOperationResult InspectionAcquisitionController::openDevice(
    int deviceIndex)
{
    return m_cameraDevice
            ? m_cameraDevice->openDevice(deviceIndex)
            : CameraOperationResult(-1);
}

CameraOperationResult InspectionAcquisitionController::closeCamera()
{
    return m_cameraDevice
            ? m_cameraDevice->close()
            : CameraOperationResult(-1);
}

CameraOperationResult InspectionAcquisitionController::setEnumValue(
    const char *key,
    unsigned int value)
{
    return m_cameraDevice
            ? m_cameraDevice->setEnumValue(key, value)
            : CameraOperationResult(-1);
}

CameraOperationResult InspectionAcquisitionController::setFloatValue(
    const char *key,
    float value)
{
    return m_cameraDevice
            ? m_cameraDevice->setFloatValue(key, value)
            : CameraOperationResult(-1);
}

CameraOperationResult InspectionAcquisitionController::getFloatValue(
    const char *key,
    CameraFloatValue *value)
{
    return m_cameraDevice
            ? m_cameraDevice->getFloatValue(key, value)
            : CameraOperationResult(-1);
}

CameraOperationResult
InspectionAcquisitionController::registerImageCallback()
{
    return m_cameraDevice
            ? m_cameraDevice->registerImageCallback()
            : CameraOperationResult(-1);
}

CameraOperationResult InspectionAcquisitionController::startGrabbing()
{
    return m_cameraDevice
            ? m_cameraDevice->startGrabbing()
            : CameraOperationResult(-1);
}

void InspectionAcquisitionController::requestCameraStop()
{
    if (m_cameraDevice) {
        m_cameraDevice->requestStop();
    }
}

InspectionCameraParameterResult
InspectionAcquisitionController::queryExposureRange()
{
    return m_cameraOperations.queryExposureRange();
}

InspectionCameraParameterResult
InspectionAcquisitionController::applyExposure(int exposureValue)
{
    return m_cameraOperations.applyExposure(exposureValue);
}

InspectionCameraParameterResult
InspectionAcquisitionController::queryGainRange()
{
    return m_cameraOperations.queryGainRange();
}

InspectionCameraParameterResult
InspectionAcquisitionController::applyGain(int gainValue)
{
    return m_cameraOperations.applyGain(gainValue);
}

InspectionCameraParameterResult
InspectionAcquisitionController::applySavedExposure(
    int savedExposure,
    const std::function<bool(int, QString *)> &persistAdjustedExposure)
{
    return m_cameraOperations.applySavedExposure(
        savedExposure, persistAdjustedExposure);
}

InspectionCameraOpenResult
InspectionAcquisitionController::openFirstCamera(
    int savedExposure,
    const std::function<bool(int, QString *)> &persistAdjustedExposure,
    int knownDeviceCount)
{
    return m_cameraOperations.openFirstCamera(
        savedExposure,
        persistAdjustedExposure,
        [this]() { ensureWorkersReady(); },
        knownDeviceCount);
}

InspectionCameraStartResult
InspectionAcquisitionController::applyCameraStart(
    InspectionAcquisitionKind acquisitionKind,
    float gain,
    const std::function<bool(QString *)> &applyExposure)
{
    InspectionCameraStartRequest request;
    request.cameraDevice = m_cameraDevice.get();
    request.acquisitionKind = acquisitionKind;
    request.gain = gain;
    request.applyExposure = applyExposure;
    return InspectionCameraStartTransition::apply(request);
}

InspectionCameraRecoveryResult
InspectionAcquisitionController::recoverCamera(
    bool recoveryRequired,
    bool cameraWasOpen,
    const std::function<bool(QString *, QString *)> &applySavedExposure)
{
    InspectionCameraRecoveryRequest request;
    request.cameraDevice = m_cameraDevice.get();
    request.recoveryRequired = recoveryRequired && hasCamera();
    request.cameraWasOpen = cameraWasOpen;
    request.applySavedExposure = applySavedExposure;
    return InspectionCameraRecoveryTransition::apply(request);
}

void InspectionAcquisitionController::configureSoftwareWorker(
    const InspectionRunPlan &plan,
    const std::vector<WordTrackingProfile> &wordProfiles,
    const std::vector<cv::Point2f> &datePolygon,
    const cv::Rect2d &trackingBox,
    const cv::Mat &trackingTemplate)
{
    ensureWorkersReady();
    InspectionWorkerConfigurator::configureSoftwareWorker(
                m_softwareWorker,
                plan,
                wordProfiles,
                datePolygon,
                trackingBox,
                trackingTemplate);
}

void InspectionAcquisitionController::configureHardwareWorker(
    const InspectionRunPlan &plan,
    const std::vector<WordTrackingProfile> &wordProfiles,
    const std::vector<cv::Point2f> &datePolygon,
    const cv::Rect2d &trackingBox,
    const cv::Mat &trackingTemplate)
{
    reinitializeHardwareWorker();
    InspectionWorkerConfigurator::configureHardwareWorker(
                m_hardwareWorker,
                plan,
                wordProfiles,
                datePolygon,
                trackingBox,
                trackingTemplate);
    applyThreadSettings(m_angle, m_colorChannel, m_delayText);
}

void InspectionAcquisitionController::applyThreadSettings(
    int angle,
    int colorChannel,
    const QString &delayText)
{
    m_angle = angle;
    m_colorChannel = colorChannel;
    m_delayText = delayText;
    if (m_softwareWorker) {
        m_softwareWorker->receiveangle(angle);
        m_softwareWorker->receivecolorchannel1(colorChannel);
        m_softwareWorker->received(delayText);
    }
    if (m_hardwareWorker) {
        m_hardwareWorker->receiveangle1(angle);
        m_hardwareWorker->receivecolorchannel(colorChannel);
        m_hardwareWorker->received(delayText);
    }
}

bool InspectionAcquisitionController::startSoftwareWorker()
{
    ensureWorkersReady();
    if (!m_softwareWorker || m_softwareWorker->isRunning()) {
        return false;
    }
    m_softwareWorker->setCameraDevice(m_cameraDevice);
    m_softwareWorker->getImagePtr(m_imageBuffer.get());
    m_softwareWorker->start();
    return true;
}

bool InspectionAcquisitionController::startHardwareWorker()
{
    if (!m_hardwareWorker || m_hardwareWorker->isRunning()) {
        return false;
    }
    m_hardwareWorker->start();
    return !m_hardwareWorker->wait(100);
}

void InspectionAcquisitionController::requestSoftwareStop()
{
    if (m_softwareWorker) {
        m_softwareWorker->requestStop();
        m_softwareWorker->stop();
    }
}

void InspectionAcquisitionController::requestHardwareStop()
{
    if (m_hardwareWorker) {
        m_hardwareWorker->requestStop();
    }
}

bool InspectionAcquisitionController::waitForSoftware(
    unsigned long milliseconds)
{
    return !m_softwareWorker
            || !m_softwareWorker->isRunning()
            || m_softwareWorker->wait(milliseconds);
}

bool InspectionAcquisitionController::waitForHardware(
    unsigned long milliseconds)
{
    return !m_hardwareWorker
            || !m_hardwareWorker->isRunning()
            || m_hardwareWorker->wait(milliseconds);
}

bool InspectionAcquisitionController::reinitializeSoftwareWorker()
{
    if (m_softwareWorker) {
        requestSoftwareStop();
        if (!waitForSoftware(2000)) {
            qWarning() << "[ACQUISITION] software worker did not stop within 2 seconds";
            return false;
        }
        delete m_softwareWorker;
        m_softwareWorker = nullptr;
    }
    createSoftwareWorker();
    return m_softwareWorker != nullptr;
}

bool InspectionAcquisitionController::reinitializeHardwareWorker()
{
    if (m_hardwareWorker) {
        requestHardwareStop();
        if (!waitForHardware(3000)) {
            qWarning() << "[ACQUISITION] hardware worker did not stop within 3 seconds";
            return false;
        }
        delete m_hardwareWorker;
        m_hardwareWorker = nullptr;
    }
    if (!m_cameraDevice) {
        return false;
    }
    createHardwareWorker();
    return m_hardwareWorker != nullptr;
}

void InspectionAcquisitionController::ensureWorkersReady()
{
    if (!m_softwareWorker || m_softwareWorker->isRunning()) {
        reinitializeSoftwareWorker();
    }
}

bool InspectionAcquisitionController::startTemplatePreview(
    quint64 sessionId,
    int angle,
    int colorChannel)
{
    ensureWorkersReady();
    if (!m_softwareWorker || !m_cameraDevice
            || m_softwareWorker->isRunning()) {
        return false;
    }
    m_softwareWorker->setCameraDevice(m_cameraDevice);
    m_softwareWorker->getImagePtr(m_imageBuffer.get());
    m_softwareWorker->receiveangle(angle);
    m_softwareWorker->receivecolorchannel1(colorChannel);
    m_softwareWorker->setTemplatePreviewMode(true, sessionId);
    m_softwareWorker->start();
    return true;
}

bool InspectionAcquisitionController::stopTemplatePreview(
    quint64 invalidatedSessionId,
    int waitTimeMs)
{
    if (!m_softwareWorker) {
        return true;
    }
    m_softwareWorker->setTemplatePreviewMode(
                false,
                invalidatedSessionId);
    requestSoftwareStop();
    if (!waitForSoftware(static_cast<unsigned long>(waitTimeMs))) {
        qWarning() << "[TEMPLATE_PREVIEW] worker did not stop within"
                   << waitTimeMs << "ms";
        return false;
    }
    return true;
}

void InspectionAcquisitionController::disableTemplatePreview(
    quint64 invalidatedSessionId)
{
    if (m_softwareWorker) {
        m_softwareWorker->setTemplatePreviewMode(
                    false,
                    invalidatedSessionId);
    }
}

InspectionAcquisitionStopResult
InspectionAcquisitionController::stopInspection()
{
    MyThread *softwareWorker = m_softwareWorker;
    CameraThread *hardwareWorker = m_hardwareWorker;
    InspectionAcquisitionStopRequest request;
    request.software.isPresent = softwareWorker != nullptr;
    request.software.isRunning = [softwareWorker]() {
        return softwareWorker && softwareWorker->isRunning();
    };
    request.software.requestStop = [softwareWorker]() {
        if (softwareWorker) {
            softwareWorker->requestStop();
        }
    };
    request.software.stopLoop = [softwareWorker]() {
        if (softwareWorker) {
            softwareWorker->stop();
        }
    };
    request.software.wait = [softwareWorker](unsigned long milliseconds) {
        return !softwareWorker || softwareWorker->wait(milliseconds);
    };
    request.software.afterStopped = [softwareWorker]() {
        if (softwareWorker) {
            softwareWorker->stopTracking();
        }
    };

    request.hardware.isPresent = hardwareWorker != nullptr;
    request.hardware.waitWhenPresent = true;
    request.hardware.repeatStopRequestAfterPrepare = true;
    request.hardware.isRunning = [hardwareWorker]() {
        return hardwareWorker && hardwareWorker->isRunning();
    };
    request.hardware.requestStop = [hardwareWorker]() {
        if (hardwareWorker) {
            hardwareWorker->requestStop();
        }
    };
    request.hardware.prepareForWait = [this, hardwareWorker]() {
        if (hardwareWorker) {
            disconnect(hardwareWorker, nullptr, this, nullptr);
        }
    };
    request.hardware.wait = [hardwareWorker](unsigned long milliseconds) {
        return !hardwareWorker || hardwareWorker->wait(milliseconds);
    };
    request.hardware.afterStopped = [this, hardwareWorker]() {
        if (!hardwareWorker) {
            return;
        }
        hardwareWorker->stopTracking();
        delete hardwareWorker;
        if (m_hardwareWorker == hardwareWorker) {
            m_hardwareWorker = nullptr;
        }
    };
    return InspectionAcquisitionStopCoordinator::stop(request);
}

void InspectionAcquisitionController::stopForApplicationExit(
    unsigned long waitTimeMs)
{
    requestSoftwareStop();
    requestHardwareStop();
    requestCameraStop();
    waitForSoftware(waitTimeMs);
    waitForHardware(waitTimeMs);
}

void InspectionAcquisitionController::shutdown(unsigned long waitTimeMs)
{
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;
    requestSoftwareStop();
    requestHardwareStop();
    const bool softwareStopped = waitForSoftware(waitTimeMs);
    const bool hardwareStopped = waitForHardware(waitTimeMs);
    if (softwareStopped) {
        delete m_softwareWorker;
        m_softwareWorker = nullptr;
    } else if (m_softwareWorker) {
        disconnect(m_softwareWorker, nullptr, this, nullptr);
        m_softwareWorker->setParent(nullptr);
        m_softwareWorker = nullptr;
    }
    if (hardwareStopped) {
        delete m_hardwareWorker;
        m_hardwareWorker = nullptr;
    } else if (m_hardwareWorker) {
        disconnect(m_hardwareWorker, nullptr, this, nullptr);
        m_hardwareWorker->setParent(nullptr);
        m_hardwareWorker = nullptr;
    }
    if (softwareStopped && hardwareStopped && m_cameraDevice) {
        m_cameraDevice->close();
        m_cameraOperations.setCameraDevice(nullptr);
        m_cameraDevice.reset();
    }
    if (softwareStopped) {
        m_imageBuffer.reset();
    } else {
        // Match the legacy shutdown safety rule: a worker that missed the
        // deadline may still hold this raw buffer pointer, so intentionally
        // leave it alive instead of creating a use-after-free during exit.
        m_imageBuffer.release();
    }
}

bool InspectionAcquisitionController::hasCurrentImage() const
{
    return m_imageBuffer && !m_imageBuffer->empty();
}

cv::Mat InspectionAcquisitionController::currentImageClone() const
{
    return hasCurrentImage()
            ? m_imageBuffer->clone()
            : cv::Mat();
}

void InspectionAcquisitionController::replaceCurrentImage(
    const cv::Mat &image)
{
    if (!m_imageBuffer) {
        m_imageBuffer.reset(new cv::Mat);
    }
    *m_imageBuffer = image.clone();
}

void InspectionAcquisitionController::createSoftwareWorker()
{
    m_shutdown = false;
    m_softwareWorker = new MyThread(this);
    m_softwareWorker->setCameraDevice(m_cameraDevice);
    m_softwareWorker->getImagePtr(m_imageBuffer.get());
    connectSoftwareWorker(m_softwareWorker);
    applyThreadSettings(m_angle, m_colorChannel, m_delayText);
}

void InspectionAcquisitionController::createHardwareWorker()
{
    m_shutdown = false;
    m_hardwareWorker = new CameraThread(this, m_cameraDevice);
    connectHardwareWorker(m_hardwareWorker);
}

void InspectionAcquisitionController::connectSoftwareWorker(
    MyThread *worker)
{
    connect(worker,
            &MyThread::signal_sendWholeFrameForDetection,
            this,
            [this](cv::Mat image) {
        if (m_runtimeController
                && (m_runtimeController->isDetectionWorkerActiveForMode(3)
                    || m_runtimeController->state()
                       == InspectionRuntimeState::Fault)) {
            submitSoftwareFrame(image);
        }
    },
    Qt::DirectConnection);
    connect(worker,
            &MyThread::signal_sendForDetection,
            this,
            [this](cv::Mat image, DetectionPose pose) {
        if (!m_runtimeController) {
            return;
        }
        if (m_runtimeController->state() == InspectionRuntimeState::Fault) {
            submitSoftwarePositionedFrame(image, pose);
            return;
        }
        const int mode = m_runtimeController->detectionWorkerModeIndex();
        if (m_runtimeController->isDetectionWorkerActive()
                && (mode == 0 || mode == 1 || mode == 2 || mode == 4)) {
            submitSoftwarePositionedFrame(image, pose);
        }
    },
    Qt::DirectConnection);
    connect(worker,
            &MyThread::signal_messImage,
            this,
            [this](cv::Mat image) {
        if (!streamingSuppressed()) {
            queueStreamingFrame(image);
        }
    },
    Qt::DirectConnection);
    connect(worker,
            &MyThread::signal_boxesSelected,
            this,
            [this](DetectionPose pose) {
        if (!streamingSuppressed()) {
            queueTrackingPose(pose);
        }
    },
    Qt::DirectConnection);
    connect(worker,
            &MyThread::signal_cleanlabel,
            this,
            [this]() {
        if (m_callbacks.clearResultText) {
            m_callbacks.clearResultText();
        }
    },
    Qt::QueuedConnection);
    connect(worker,
            &MyThread::signal_templatePreviewImage,
            this,
            [this, worker](cv::Mat image, quint64 sessionId) {
        if (m_callbacks.presentTemplatePreview) {
            m_callbacks.presentTemplatePreview(sessionId, image);
        }
        if (worker == m_softwareWorker) {
            worker->acknowledgeTemplatePreviewFrame(sessionId);
        }
    },
    Qt::QueuedConnection);
    connect(worker,
            &MyThread::signal_templatePreviewError,
            this,
            [this](const QString &reason, quint64 sessionId) {
        if (m_callbacks.reportTemplatePreviewError) {
            m_callbacks.reportTemplatePreviewError(sessionId, reason);
        }
    },
    Qt::QueuedConnection);
    connect(worker,
            &QThread::finished,
            this,
            [this, worker]() {
        if (worker == m_softwareWorker
                && m_callbacks.softwareThreadFinished) {
            m_callbacks.softwareThreadFinished();
        }
    },
    Qt::QueuedConnection);
}

void InspectionAcquisitionController::connectHardwareWorker(
    CameraThread *worker)
{
    connect(worker,
            &CameraThread::signal_sendWholeFrameForDetection,
            this,
            [this](cv::Mat image) {
        submitHardwareFrame(image);
    },
    Qt::DirectConnection);
    connect(worker,
            &CameraThread::signal_sendForDetection,
            this,
            [this](cv::Mat image, DetectionPose pose) {
        submitHardwarePositionedFrame(image, pose);
    },
    Qt::DirectConnection);
    connect(worker,
            &CameraThread::signal_messImage,
            this,
            [this](cv::Mat image) {
        if (!streamingSuppressed()) {
            queueStreamingFrame(image);
        }
    },
    Qt::DirectConnection);
    connect(worker,
            &CameraThread::signal_boxesSelected,
            this,
            [this](DetectionPose pose) {
        if (!streamingSuppressed()) {
            queueTrackingPose(pose);
        }
    },
    Qt::DirectConnection);
    connect(worker,
            &CameraThread::signal_cleanlabel,
            this,
            [this]() {
        if (m_callbacks.clearResultText) {
            m_callbacks.clearResultText();
        }
    },
    Qt::QueuedConnection);
    connect(worker,
            &QThread::finished,
            this,
            [this, worker]() {
        if (worker == m_hardwareWorker
                && m_callbacks.hardwareThreadFinished) {
            m_callbacks.hardwareThreadFinished();
        }
    },
    Qt::QueuedConnection);
}

bool InspectionAcquisitionController::streamingSuppressed() const
{
    return m_callbacks.suppressStreamingFrame
            && m_callbacks.suppressStreamingFrame();
}

void InspectionAcquisitionController::queueStreamingFrame(
    const cv::Mat &image)
{
    QMetaObject::invokeMethod(
                this,
                [this, image]() {
        if (m_callbacks.presentStreamingFrame) {
            m_callbacks.presentStreamingFrame(image);
        }
    },
    Qt::QueuedConnection);
}

void InspectionAcquisitionController::queueTrackingPose(
    const DetectionPose &pose)
{
    QMetaObject::invokeMethod(
                this,
                [this, pose]() {
        if (m_callbacks.presentTrackingPose) {
            m_callbacks.presentTrackingPose(pose);
        }
    },
    Qt::QueuedConnection);
}

void InspectionAcquisitionController::submitSoftwareFrame(
    const cv::Mat &image)
{
    if (image.empty() || !m_runtimeController) {
        return;
    }
    if (m_runtimeController->state() == InspectionRuntimeState::Fault) {
        m_runtimeController->acceptFrame(image);
        return;
    }
    if (!m_runtimeController->isDetectionWorkerActive()) {
        return;
    }
    const std::shared_ptr<const FrameData> frame =
            m_runtimeController->acceptFrame(image);
    if (frame && !m_runtimeController->submitDetectionFrame(frame)) {
        qDebug() << "[DETECTION_WORKER] software frame rejected"
                 << frame->productKey.runId
                 << frame->productKey.sequence;
    }
}

void InspectionAcquisitionController::submitSoftwarePositionedFrame(
    const cv::Mat &image,
    const DetectionPose &pose)
{
    if (image.empty() || !m_runtimeController) {
        return;
    }
    if (m_runtimeController->state() == InspectionRuntimeState::Fault) {
        m_runtimeController->acceptFrame(image);
        return;
    }
    if (!m_runtimeController->isDetectionWorkerActive()) {
        return;
    }
    const std::shared_ptr<const FrameData> frame =
            m_runtimeController->acceptFrame(image);
    if (frame && !m_runtimeController->submitDetectionWorkItem(
                makeDetectionWorkItem(frame, pose))) {
        qDebug() << "[DETECTION_WORKER] positioned frame rejected"
                 << frame->productKey.runId
                 << frame->productKey.sequence;
    }
}

void InspectionAcquisitionController::submitHardwareFrame(
    const cv::Mat &image)
{
    if (image.empty() || !m_runtimeController) {
        return;
    }
    if (m_runtimeController->state() == InspectionRuntimeState::Fault) {
        m_runtimeController->acceptFrame(image);
        return;
    }
    if (!m_runtimeController->isDetectionWorkerActiveForMode(3)) {
        return;
    }
    const std::shared_ptr<const FrameData> frame =
            m_runtimeController->acceptFrame(image);
    if (!frame) {
        return;
    }
    const DetectionWorkSubmissionResult submission =
            m_runtimeController->trySubmitDetectionFrame(frame);
    if (submission == DetectionWorkSubmissionResult::QueueFull) {
        enterQueueOverflowFault(frame);
    } else if (submission != DetectionWorkSubmissionResult::Accepted) {
        qDebug() << "[DETECTION_WORKER] hardware frame rejected"
                 << static_cast<int>(submission)
                 << frame->productKey.runId
                 << frame->productKey.sequence;
    }
}

void InspectionAcquisitionController::submitHardwarePositionedFrame(
    const cv::Mat &image,
    const DetectionPose &pose)
{
    if (image.empty() || !m_runtimeController) {
        return;
    }
    if (m_runtimeController->state() == InspectionRuntimeState::Fault) {
        m_runtimeController->acceptFrame(image);
        return;
    }
    const int mode = m_runtimeController->detectionWorkerModeIndex();
    if (!m_runtimeController->isDetectionWorkerActive()
            || (mode != 0 && mode != 1 && mode != 2 && mode != 4)) {
        return;
    }
    const std::shared_ptr<const FrameData> frame =
            m_runtimeController->acceptFrame(image);
    if (!frame) {
        return;
    }
    const DetectionWorkSubmissionResult submission =
            m_runtimeController->trySubmitDetectionWorkItem(
                makeDetectionWorkItem(frame, pose));
    if (submission == DetectionWorkSubmissionResult::QueueFull) {
        enterQueueOverflowFault(frame);
    } else if (submission != DetectionWorkSubmissionResult::Accepted) {
        qDebug() << "[DETECTION_WORKER] positioned hardware frame rejected"
                 << static_cast<int>(submission)
                 << frame->productKey.runId
                 << frame->productKey.sequence;
    }
}

void InspectionAcquisitionController::enterQueueOverflowFault(
    const std::shared_ptr<const FrameData> &frame) const
{
    if (!frame || !m_runtimeController || !m_callbacks.enterFault) {
        return;
    }
    m_callbacks.enterFault(
                InspectionFaultReason::HardTriggerQueueOverflow,
                QStringLiteral(
                    "\u786c\u89e6\u53d1 FIFO \u65e0\u6cd5\u63a5\u6536\u4ea7\u54c1 %1/%2\uff0c\u961f\u5217\u5bb9\u91cf %3\u3002")
                .arg(frame->productKey.runId)
                .arg(frame->productKey.sequence)
                .arg(static_cast<qulonglong>(
                         m_runtimeController->detectionWorkerQueueCapacity())));
}
