#include "MultiCameraUnit.h"

#include <QElapsedTimer>
#include <QMutexLocker>
#include <QThread>
#include <chrono>
#include <mutex>

namespace {
qint64 monotonicTimestampUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

MultiCameraUnit::MultiCameraUnit(int cameraIndex, QObject *parent)
    : QObject(parent)
{
    m_deviceConfig.cameraIndex = cameraIndex;
    fillDeviceConfigDefaults();
}

MultiCameraUnit::~MultiCameraUnit()
{
    close();
}

int MultiCameraUnit::cameraIndex() const
{
    QMutexLocker locker(&m_mutex);
    return m_deviceConfig.cameraIndex;
}

CameraDeviceConfig MultiCameraUnit::deviceConfig() const
{
    QMutexLocker locker(&m_mutex);
    return m_deviceConfig;
}

CameraRuntimeConfig MultiCameraUnit::runtimeConfig() const
{
    QMutexLocker locker(&m_mutex);
    return m_runtimeConfig;
}

bool MultiCameraUnit::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_opened;
}

bool MultiCameraUnit::isGrabbing() const
{
    QMutexLocker locker(&m_mutex);
    return m_grabbing;
}

bool MultiCameraUnit::isDeviceConnected() const
{
    QMutexLocker locker(&m_mutex);
    if (!m_camera || !m_opened) {
        return false;
    }

    return m_camera->IsDeviceConnected();
}

quint64 MultiCameraUnit::frameId() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastFrameSeq;
}

QString MultiCameraUnit::lastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

CameraFrame MultiCameraUnit::lastFrame() const
{
    QMutexLocker locker(&m_mutex);
    CameraFrame frame = m_lastFrame;
    frame.image = m_lastFrame.image.clone();
    return frame;
}

void MultiCameraUnit::setDeviceConfig(const CameraDeviceConfig& config)
{
    QMutexLocker locker(&m_mutex);
    const int oldIndex = m_deviceConfig.cameraIndex;
    m_deviceConfig = config;
    if (m_deviceConfig.cameraIndex < 0) {
        m_deviceConfig.cameraIndex = oldIndex;
    }
    fillDeviceConfigDefaults();
}

void MultiCameraUnit::setRuntimeConfig(const CameraRuntimeConfig& config)
{
    QMutexLocker locker(&m_mutex);
    m_runtimeConfig = config;
}

bool MultiCameraUnit::open(MV_CC_DEVICE_INFO *deviceInfo, const CameraDeviceConfig& deviceConfig)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_opened) {
            return true;
        }
    }

    if (deviceInfo == nullptr) {
        return setError(QStringLiteral("Open camera failed: device info is null."));
    }

    std::unique_ptr<CMvCamera> camera(new CMvCamera());
    const int ret = camera->Open(deviceInfo);
    if (ret != MV_OK) {
        return setError(sdkErrorString(QStringLiteral("Open camera"), ret));
    }

    {
        QMutexLocker locker(&m_mutex);
        const int oldIndex = m_deviceConfig.cameraIndex;
        m_deviceConfig = deviceConfig;
        if (m_deviceConfig.cameraIndex < 0) {
            m_deviceConfig.cameraIndex = oldIndex;
        }
        if (m_deviceConfig.serialNumber.isEmpty()) {
            m_deviceConfig.serialNumber = serialNumberFromDeviceInfo(deviceInfo);
        }
        fillDeviceConfigDefaults();

        m_camera = std::move(camera);
        m_opened = true;
        m_grabbing = false;
        m_callbackRegistered = false;
        m_lastError.clear();
        m_lastFrameSeq = 0;
        m_waitBaseFrameSeq = 0;
        m_lastShotId = 0;
        m_lastFrame = CameraFrame();
        m_lastFrame.cameraIndex = m_deviceConfig.cameraIndex;
        m_lastFrame.serialNumber = m_deviceConfig.serialNumber;
    }

    emit stateChanged();
    if (!applyRuntimeConfig(runtimeConfig())) {
        close();
        return false;
    }

    return true;
}

void MultiCameraUnit::close()
{
    std::unique_ptr<CMvCamera> cameraToClose;
    bool shouldClose = false;

    {
        QMutexLocker locker(&m_mutex);
        if (!m_camera) {
            m_opened = false;
            m_grabbing = false;
            return;
        }

        cameraToClose = std::move(m_camera);
        shouldClose = m_opened;
        m_opened = false;
        m_grabbing = false;
        m_callbackRegistered = false;
        m_lastFrameSeq = 0;
        m_waitBaseFrameSeq = 0;
    }

    if (shouldClose) {
        cameraToClose->StopGrabbing();
        cameraToClose->Close();
    }

    emit stateChanged();
}

bool MultiCameraUnit::start()
{
    CMvCamera *camera = nullptr;
    bool canStart = true;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_opened || !m_camera) {
            canStart = false;
        } else if (m_grabbing) {
            return true;
        } else {
            camera = m_camera.get();
        }
    }

    if (!canStart) {
        return setError(QStringLiteral("Start grabbing failed: camera is not open."));
    }

    if (!applyRuntimeConfig(runtimeConfig())) {
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);
        camera = m_camera.get();
        if (!camera) {
            locker.unlock();
            return setError(QStringLiteral("Start grabbing failed: camera was closed."));
        }
        if (!m_callbackRegistered) {
            const int registerRet = camera->RegisterImageCallBack();
            if (registerRet != MV_OK) {
                locker.unlock();
                return setError(sdkErrorString(QStringLiteral("Register image callback"), registerRet));
            }
            m_callbackRegistered = true;
        }
    }

    const int ret = camera->StartGrabbing();
    if (ret != MV_OK) {
        return setError(sdkErrorString(QStringLiteral("Start grabbing"), ret));
    }

    {
        QMutexLocker locker(&m_mutex);
        m_grabbing = true;
        m_waitBaseFrameSeq = camera->m_frameseq.load();
        m_lastError.clear();
    }

    emit stateChanged();
    return true;
}

void MultiCameraUnit::stop()
{
    CMvCamera *camera = nullptr;
    bool shouldStop = false;

    {
        QMutexLocker locker(&m_mutex);
        camera = m_camera.get();
        shouldStop = (camera != nullptr && m_grabbing);
        m_grabbing = false;
        if (camera) {
            m_waitBaseFrameSeq = camera->m_frameseq.load();
        }
    }

    if (shouldStop) {
        const int ret = camera->StopGrabbing();
        if (ret != MV_OK) {
            setError(sdkErrorString(QStringLiteral("Stop grabbing"), ret));
        }
    }

    emit stateChanged();
}

bool MultiCameraUnit::setExposure(double exposureUs)
{
    if (exposureUs < 0.0) {
        return setError(QStringLiteral("Set exposure failed: exposure must be greater than or equal to 0."));
    }

    CMvCamera *camera = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        m_runtimeConfig.exposureUs = exposureUs;
        camera = m_camera.get();
        if (!m_opened || !camera || exposureUs <= 0.0) {
            return true;
        }
    }

    const int ret = camera->SetFloatValue("ExposureTime", static_cast<float>(exposureUs));
    if (ret != MV_OK) {
        return setError(sdkErrorString(QStringLiteral("Set exposure"), ret));
    }

    return true;
}

bool MultiCameraUnit::setGain(double gain)
{
    if (gain < 0.0) {
        return setError(QStringLiteral("Set gain failed: gain must be greater than or equal to 0."));
    }

    CMvCamera *camera = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        m_runtimeConfig.gain = gain;
        camera = m_camera.get();
        if (!m_opened || !camera) {
            return true;
        }
    }

    const int ret = camera->SetFloatValue("Gain", static_cast<float>(gain));
    if (ret != MV_OK) {
        return setError(sdkErrorString(QStringLiteral("Set gain"), ret));
    }

    return true;
}

bool MultiCameraUnit::setTriggerMode(MultiCameraTriggerMode triggerMode)
{
    CMvCamera *camera = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        m_runtimeConfig.triggerMode = triggerMode;
        camera = m_camera.get();
        if (!m_opened || !camera) {
            return true;
        }
    }

    int ret = MV_OK;
    switch (triggerMode) {
    case MultiCameraTriggerMode::Continuous:
        ret = camera->SetEnumValue("TriggerMode", 0);
        break;
    case MultiCameraTriggerMode::Software:
        ret = camera->SetEnumValue("TriggerMode", 1);
        if (ret == MV_OK) {
            ret = camera->SetEnumValue("TriggerSource", 7);
        }
        break;
    case MultiCameraTriggerMode::Hardware:
        ret = camera->SetEnumValue("TriggerMode", 1);
        if (ret == MV_OK) {
            ret = camera->SetEnumValue("TriggerSource", 0);
        }
        break;
    }

    if (ret != MV_OK) {
        return setError(sdkErrorString(QStringLiteral("Set trigger mode"), ret));
    }

    return true;
}

bool MultiCameraUnit::applyRuntimeConfig(const CameraRuntimeConfig& config)
{
    {
        QMutexLocker locker(&m_mutex);
        m_runtimeConfig = config;
    }

    bool ok = true;
    if (config.exposureUs > 0.0) {
        ok = setExposure(config.exposureUs) && ok;
    }
    if (config.gain > 0.0) {
        ok = setGain(config.gain) && ok;
    }
    ok = setTriggerMode(config.triggerMode) && ok;
    return ok;
}

bool MultiCameraUnit::triggerOnce(quint64 shotId)
{
    CMvCamera *camera = nullptr;
    bool canTrigger = true;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_opened || !m_grabbing || !m_camera) {
            canTrigger = false;
        } else {
            camera = m_camera.get();
        }
    }

    if (!canTrigger) {
        return setError(QStringLiteral("Soft trigger failed: camera is not grabbing."));
    }

    if (runtimeConfig().triggerMode != MultiCameraTriggerMode::Software) {
        if (!setTriggerMode(MultiCameraTriggerMode::Software)) {
            return false;
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        m_lastShotId = shotId;
        m_waitBaseFrameSeq = camera->m_frameseq.load();
    }

    const int ret = camera->CommandExecute("TriggerSoftware");
    if (ret != MV_OK) {
        return setError(sdkErrorString(QStringLiteral("Execute software trigger"), ret));
    }

    return true;
}

bool MultiCameraUnit::grabFrame(CameraFrame& frame, quint64 shotId, int timeoutMs)
{
    frame = CameraFrame();

    CMvCamera *camera = nullptr;
    quint64 baseFrameSeq = 0;
    bool canGrab = true;
    int cameraIndex = -1;
    QString serialNumber;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_opened || !m_grabbing || !m_camera) {
            canGrab = false;
            cameraIndex = m_deviceConfig.cameraIndex;
            serialNumber = m_deviceConfig.serialNumber;
        } else {
            camera = m_camera.get();
            baseFrameSeq = (m_waitBaseFrameSeq > 0) ? m_waitBaseFrameSeq : m_lastFrameSeq;
            if (shotId == 0) {
                shotId = m_lastShotId;
            }
        }
    }

    if (!canGrab) {
        frame.cameraIndex = cameraIndex;
        frame.serialNumber = serialNumber;
        frame.shotId = shotId;
        frame.valid = false;
        frame.errorString = QStringLiteral("Grab frame failed: camera is not grabbing.");
        return setError(frame.errorString);
    }

    const int effectiveTimeoutMs = qMax(1, timeoutMs);
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() <= effectiveTimeoutMs) {
        const quint64 currentSeq = camera->m_frameseq.load();
        if (currentSeq > baseFrameSeq && copyLatestImage(camera, frame, shotId, baseFrameSeq)) {
            emit frameReady(frame);
            emit stateChanged();
            return true;
        }
        QThread::msleep(2);
    }

    frame.cameraIndex = deviceConfig().cameraIndex;
    frame.serialNumber = deviceConfig().serialNumber;
    frame.shotId = shotId;
    frame.valid = false;
    frame.errorString = QStringLiteral("Grab frame timeout.");
    return setError(frame.errorString);
}

void MultiCameraUnit::clearLastFrame()
{
    QMutexLocker locker(&m_mutex);
    m_lastFrame = CameraFrame();
    m_lastFrame.cameraIndex = m_deviceConfig.cameraIndex;
    m_lastFrame.serialNumber = m_deviceConfig.serialNumber;
    m_lastFrameSeq = 0;
    m_waitBaseFrameSeq = 0;
}

QString MultiCameraUnit::sdkErrorString(const QString& operation, int ret) const
{
    QMutexLocker locker(&m_mutex);
    return QStringLiteral("%1 failed. cameraIndex=%2, ret=%3")
            .arg(operation)
            .arg(m_deviceConfig.cameraIndex)
            .arg(ret);
}

bool MultiCameraUnit::setError(const QString& errorString)
{
    int index = -1;
    {
        QMutexLocker locker(&m_mutex);
        m_lastError = errorString;
        index = m_deviceConfig.cameraIndex;
    }

    emit errorOccurred(index, errorString);
    emit stateChanged();
    return false;
}

void MultiCameraUnit::fillDeviceConfigDefaults()
{
    if (m_deviceConfig.cameraIndex < 0) {
        m_deviceConfig.cameraIndex = 0;
    }
    if (m_deviceConfig.displayName.isEmpty()) {
        m_deviceConfig.displayName = QStringLiteral("Camera %1").arg(m_deviceConfig.cameraIndex + 1);
    }
}

QString MultiCameraUnit::serialNumberFromDeviceInfo(const MV_CC_DEVICE_INFO *deviceInfo)
{
    if (deviceInfo == nullptr) {
        return QString();
    }

    if (deviceInfo->nTLayerType == MV_GIGE_DEVICE) {
        return QString::fromLocal8Bit(reinterpret_cast<const char*>(
                                          deviceInfo->SpecialInfo.stGigEInfo.chSerialNumber));
    }

    if (deviceInfo->nTLayerType == MV_USB_DEVICE) {
        return QString::fromLocal8Bit(reinterpret_cast<const char*>(
                                          deviceInfo->SpecialInfo.stUsb3VInfo.chSerialNumber));
    }

    return QString();
}

bool MultiCameraUnit::copyLatestImage(CMvCamera *camera, CameraFrame& frame, quint64 shotId, quint64 minimumFrameSeq)
{
    if (camera == nullptr) {
        return false;
    }

    cv::Mat image;
    quint64 frameSeq = 0;

    {
        std::lock_guard<std::mutex> cameraLocker(camera->m_mutex);
        frameSeq = camera->m_frameseq.load();
        if (frameSeq <= minimumFrameSeq || camera->m_image.empty()) {
            return false;
        }
        image = camera->m_image.clone();
    }

    if (image.empty()) {
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);
        frame.cameraIndex = m_deviceConfig.cameraIndex;
        frame.serialNumber = m_deviceConfig.serialNumber;
        frame.shotId = shotId;
        frame.frameId = frameSeq;
        frame.timestampUs = monotonicTimestampUs();
        frame.image = image;
        frame.valid = true;
        frame.errorString.clear();

        m_lastFrame = frame;
        m_lastFrameSeq = frameSeq;
        m_waitBaseFrameSeq = frameSeq;
        m_lastShotId = shotId;
        m_lastError.clear();
    }

    return true;
}
