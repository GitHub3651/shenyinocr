#include "MultiCameraController.h"

#include <QElapsedTimer>
#include <QMutexLocker>
#include <QtGlobal>
#include <cstring>

MultiCameraController::MultiCameraController(QObject *parent)
    : QObject(parent),
      m_syncManager(2)
{
    for (int i = 0; i < 2; ++i) {
        MultiCameraUnit *unit = new MultiCameraUnit(i, this);
        connect(unit, &MultiCameraUnit::stateChanged,
                this, &MultiCameraController::statusChanged);
        connect(unit, &MultiCameraUnit::errorOccurred,
                this, &MultiCameraController::onUnitError);
        m_units.push_back(unit);

        CameraDeviceConfig config;
        config.cameraIndex = i;
        config.displayName = QStringLiteral("Camera %1").arg(i + 1);
        m_deviceConfigs.push_back(config);
    }
}

MultiCameraController::~MultiCameraController()
{
    closeAll();
}

bool MultiCameraController::openAll()
{
    if (!ensureDeviceList()) {
        return false;
    }

    QVector<MV_CC_DEVICE_INFO> devices = resolveOpenDeviceInfos();
    if (devices.size() != expectedCameraCount()) {
        return setControllerError(QStringLiteral("Open all cameras failed: need %1 devices, got %2.")
                                          .arg(expectedCameraCount())
                                          .arg(devices.size()));
    }

    for (int i = 0; i < m_units.size(); ++i) {
        CameraDeviceConfig config = configFromDeviceInfo(i, &devices[i]);
        if (!m_units[i]->open(&devices[i], config)) {
            const QString errorString = QStringLiteral("Open all cameras failed: %1")
                    .arg(m_units[i]->lastError());
            for (int j = 0; j <= i && j < m_units.size(); ++j) {
                m_units[j]->close();
            }
            return setControllerError(errorString);
        }
    }

    setControllerState(MultiCameraSyncState::Idle);
    return true;
}

void MultiCameraController::closeAll()
{
    stopAll();
    for (MultiCameraUnit *unit : m_units) {
        unit->close();
    }

    {
        QMutexLocker locker(&m_mutex);
        m_pendingShotId = 0;
    }
    setControllerState(MultiCameraSyncState::Idle);
}

bool MultiCameraController::startAll()
{
    if (!allUnitsOpen() && !openAll()) {
        return false;
    }

    for (int i = 0; i < m_units.size(); ++i) {
        if (!m_units[i]->start()) {
            const QString errorString = QStringLiteral("Start all cameras failed: %1")
                    .arg(m_units[i]->lastError());
            for (int j = 0; j <= i && j < m_units.size(); ++j) {
                m_units[j]->stop();
            }
            return setControllerError(errorString);
        }
    }

    setControllerState(MultiCameraSyncState::Idle);
    return true;
}

void MultiCameraController::stopAll()
{
    for (MultiCameraUnit *unit : m_units) {
        unit->stop();
    }

    {
        QMutexLocker locker(&m_mutex);
        m_pendingShotId = 0;
    }
    setControllerState(MultiCameraSyncState::Idle);
}

bool MultiCameraController::triggerOnce()
{
    if (!allUnitsGrabbing()) {
        return setControllerError(QStringLiteral("Soft trigger failed: not all cameras are grabbing."));
    }

    quint64 shotId = 0;
    bool hasPendingShot = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_pendingShotId != 0) {
            hasPendingShot = true;
        } else {
            shotId = ++m_currentShotId;
            m_pendingShotId = shotId;
            m_controllerState = MultiCameraSyncState::Waiting;
            m_lastError.clear();
        }
    }

    if (hasPendingShot) {
        return setControllerError(QStringLiteral("Soft trigger failed: previous shot has not been grabbed."));
    }

    emit statusChanged();

    for (int i = 0; i < m_units.size(); ++i) {
        if (!m_units[i]->triggerOnce(shotId)) {
            MultiCameraShot failedShot;
            const QString errorString = QStringLiteral("Soft trigger failed on camera %1: %2")
                    .arg(i + 1)
                    .arg(m_units[i]->lastError());
            m_syncManager.recordFailure(shotId,
                                        QVector<CameraFrame>(),
                                        MultiCameraSyncState::Error,
                                        errorString,
                                        failedShot);
            {
                QMutexLocker locker(&m_mutex);
                m_pendingShotId = 0;
            }
            setControllerState(MultiCameraSyncState::Error, errorString);
            return false;
        }
    }

    return true;
}

bool MultiCameraController::grabShot(MultiCameraShot& shot, int timeoutMs)
{
    shot = MultiCameraShot();

    if (!allUnitsGrabbing()) {
        const QString errorString = QStringLiteral("Grab shot failed: not all cameras are grabbing.");
        m_syncManager.recordFailure(0,
                                    QVector<CameraFrame>(),
                                    MultiCameraSyncState::Error,
                                    errorString,
                                    shot);
        setControllerState(MultiCameraSyncState::Error, errorString);
        return false;
    }

    quint64 shotId = 0;
    {
        QMutexLocker locker(&m_mutex);
        if (m_pendingShotId != 0) {
            shotId = m_pendingShotId;
            m_pendingShotId = 0;
        } else {
            shotId = ++m_currentShotId;
        }
        m_controllerState = MultiCameraSyncState::Waiting;
        m_lastError.clear();
    }

    emit statusChanged();

    QVector<CameraFrame> frames;
    frames.reserve(m_units.size());
    QElapsedTimer timer;
    timer.start();
    const int totalTimeoutMs = qMax(1, timeoutMs);

    for (int i = 0; i < m_units.size(); ++i) {
        CameraFrame frame;
        const int cameraTimeoutMs = remainingTimeoutMs(timer, totalTimeoutMs);
        const bool ok = cameraTimeoutMs > 0 && m_units[i]->grabFrame(frame, shotId, cameraTimeoutMs);
        frames.push_back(frame);
        if (!ok) {
            const QString unitError = frame.errorString.isEmpty()
                    ? m_units[i]->lastError()
                    : frame.errorString;
            const QString errorString = QStringLiteral("Grab shot %1 failed on camera %2: %3")
                    .arg(shotId)
                    .arg(i + 1)
                    .arg(unitError.isEmpty() ? QStringLiteral("unknown error") : unitError);

            if (timer.elapsed() >= totalTimeoutMs || unitError.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)) {
                m_syncManager.recordTimeout(shotId, frames, errorString, shot);
                setControllerState(MultiCameraSyncState::Timeout, errorString);
            } else {
                m_syncManager.recordFailure(shotId, frames, MultiCameraSyncState::Error, errorString, shot);
                setControllerState(MultiCameraSyncState::Error, errorString);
            }
            return false;
        }
    }

    const bool complete = m_syncManager.assembleShot(shotId, frames, shot);
    setControllerState(shot.syncState, shot.errorString);

    if (complete) {
        emit shotReady(shot);
    }

    return complete;
}

MultiCameraStatus MultiCameraController::status() const
{
    MultiCameraStatus result = m_syncManager.status();
    result.allOpened = allUnitsOpen();
    result.allGrabbing = allUnitsGrabbing();

    QVector<CameraDeviceConfig> devices;
    QVector<QString> errors;
    devices.reserve(m_units.size());
    errors.reserve(m_units.size());
    for (const MultiCameraUnit *unit : m_units) {
        devices.push_back(unit->deviceConfig());
        errors.push_back(unit->lastError());
    }

    QMutexLocker locker(&m_mutex);
    result.devices = devices;
    result.cameraErrors = errors;
    result.currentShotId = m_currentShotId;
    result.syncState = m_controllerState;
    if (!m_lastError.isEmpty()) {
        result.lastError = m_lastError;
    }
    return result;
}

QVector<CameraDeviceConfig> MultiCameraController::scanDevices()
{
    MV_CC_DEVICE_INFO_LIST deviceList;
    std::memset(&deviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    const int ret = CMvCamera::EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &deviceList);
    if (ret != MV_OK) {
        setControllerError(QStringLiteral("Scan devices failed: ret=%1").arg(ret));
        return QVector<CameraDeviceConfig>();
    }

    QVector<MV_CC_DEVICE_INFO> deviceInfos;
    QVector<CameraDeviceConfig> devices;
    deviceInfos.reserve(static_cast<int>(deviceList.nDeviceNum));
    devices.reserve(static_cast<int>(deviceList.nDeviceNum));

    for (unsigned int i = 0; i < deviceList.nDeviceNum; ++i) {
        if (deviceList.pDeviceInfo[i] == nullptr) {
            continue;
        }
        deviceInfos.push_back(*deviceList.pDeviceInfo[i]);
        devices.push_back(configFromDeviceInfo(devices.size(), &deviceInfos.last()));
    }

    {
        QMutexLocker locker(&m_mutex);
        m_deviceInfos = deviceInfos;
        m_availableDevices = devices;
    }

    emit statusChanged();
    return devices;
}

QVector<CameraDeviceConfig> MultiCameraController::availableDevices() const
{
    QMutexLocker locker(&m_mutex);
    return m_availableDevices;
}

void MultiCameraController::setDeviceConfigs(const QVector<CameraDeviceConfig>& configs)
{
    {
        QMutexLocker locker(&m_mutex);
        m_deviceConfigs.clear();
        for (int i = 0; i < expectedCameraCount(); ++i) {
            CameraDeviceConfig config = (i < configs.size()) ? configs[i] : CameraDeviceConfig();
            config.cameraIndex = i;
            if (config.displayName.isEmpty()) {
                config.displayName = QStringLiteral("Camera %1").arg(i + 1);
            }
            m_deviceConfigs.push_back(config);
            if (i < m_units.size()) {
                m_units[i]->setDeviceConfig(config);
            }
        }
    }

    emit statusChanged();
}

QVector<CameraDeviceConfig> MultiCameraController::deviceConfigs() const
{
    QMutexLocker locker(&m_mutex);
    return m_deviceConfigs;
}

void MultiCameraController::setRuntimeConfig(int cameraIndex, const CameraRuntimeConfig& config)
{
    if (cameraIndex < 0 || cameraIndex >= m_units.size()) {
        setControllerError(QStringLiteral("Set runtime config failed: invalid camera index %1.").arg(cameraIndex));
        return;
    }

    if (!m_units[cameraIndex]->applyRuntimeConfig(config)) {
        setControllerError(QStringLiteral("Set runtime config failed on camera %1: %2")
                                   .arg(cameraIndex + 1)
                                   .arg(m_units[cameraIndex]->lastError()));
        return;
    }

    emit statusChanged();
}

CameraRuntimeConfig MultiCameraController::runtimeConfig(int cameraIndex) const
{
    if (cameraIndex < 0 || cameraIndex >= m_units.size()) {
        return CameraRuntimeConfig();
    }

    return m_units[cameraIndex]->runtimeConfig();
}

void MultiCameraController::setMaxTimestampDiffUs(qint64 maxDiffUs)
{
    m_syncManager.setMaxTimestampDiffUs(maxDiffUs);
}

qint64 MultiCameraController::maxTimestampDiffUs() const
{
    return m_syncManager.maxTimestampDiffUs();
}

void MultiCameraController::setRequireEqualFrameId(bool enabled)
{
    m_syncManager.setRequireEqualFrameId(enabled);
}

bool MultiCameraController::requireEqualFrameId() const
{
    return m_syncManager.requireEqualFrameId();
}

void MultiCameraController::onUnitError(int cameraIndex, const QString& errorString)
{
    Q_UNUSED(cameraIndex)
    {
        QMutexLocker locker(&m_mutex);
        m_lastError = errorString;
    }
    emit errorOccurred(errorString);
    emit statusChanged();
}

QString MultiCameraController::serialNumberFromDeviceInfo(const MV_CC_DEVICE_INFO *deviceInfo)
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

QString MultiCameraController::userDefinedNameFromDeviceInfo(const MV_CC_DEVICE_INFO *deviceInfo)
{
    if (deviceInfo == nullptr) {
        return QString();
    }

    if (deviceInfo->nTLayerType == MV_GIGE_DEVICE) {
        return QString::fromLocal8Bit(reinterpret_cast<const char*>(
                                          deviceInfo->SpecialInfo.stGigEInfo.chUserDefinedName));
    }

    if (deviceInfo->nTLayerType == MV_USB_DEVICE) {
        return QString::fromLocal8Bit(reinterpret_cast<const char*>(
                                          deviceInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName));
    }

    return QString();
}

QString MultiCameraController::modelNameFromDeviceInfo(const MV_CC_DEVICE_INFO *deviceInfo)
{
    if (deviceInfo == nullptr) {
        return QString();
    }

    if (deviceInfo->nTLayerType == MV_GIGE_DEVICE) {
        return QString::fromLocal8Bit(reinterpret_cast<const char*>(
                                          deviceInfo->SpecialInfo.stGigEInfo.chModelName));
    }

    if (deviceInfo->nTLayerType == MV_USB_DEVICE) {
        return QString::fromLocal8Bit(reinterpret_cast<const char*>(
                                          deviceInfo->SpecialInfo.stUsb3VInfo.chModelName));
    }

    return QString();
}

CameraDeviceConfig MultiCameraController::configFromDeviceInfo(int logicalIndex,
                                                               const MV_CC_DEVICE_INFO *deviceInfo) const
{
    CameraDeviceConfig config;
    config.cameraIndex = logicalIndex;
    config.enabled = true;
    config.serialNumber = serialNumberFromDeviceInfo(deviceInfo);
    config.userDefinedName = userDefinedNameFromDeviceInfo(deviceInfo);
    config.displayName = config.userDefinedName;
    if (config.displayName.isEmpty()) {
        config.displayName = modelNameFromDeviceInfo(deviceInfo);
    }
    if (config.displayName.isEmpty()) {
        config.displayName = QStringLiteral("Camera %1").arg(logicalIndex + 1);
    }
    return config;
}

bool MultiCameraController::ensureDeviceList()
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_deviceInfos.size() >= expectedCameraCount()) {
            return true;
        }
    }

    scanDevices();

    int deviceCount = 0;
    {
        QMutexLocker locker(&m_mutex);
        deviceCount = m_deviceInfos.size();
    }

    if (deviceCount < expectedCameraCount()) {
        return setControllerError(QStringLiteral("Scan devices failed: need %1 devices, got %2.")
                                          .arg(expectedCameraCount())
                                          .arg(deviceCount));
    }

    return true;
}

QVector<MV_CC_DEVICE_INFO> MultiCameraController::resolveOpenDeviceInfos()
{
    QVector<MV_CC_DEVICE_INFO> result;
    QVector<bool> used;
    QVector<CameraDeviceConfig> configs;

    {
        QMutexLocker locker(&m_mutex);
        used.fill(false, m_deviceInfos.size());
        configs = m_deviceConfigs;

        for (int i = 0; i < expectedCameraCount(); ++i) {
            int selectedIndex = -1;
            const CameraDeviceConfig config = (i < configs.size()) ? configs[i] : CameraDeviceConfig();

            if (!config.serialNumber.isEmpty()) {
                for (int j = 0; j < m_deviceInfos.size(); ++j) {
                    if (!used[j] && serialNumberFromDeviceInfo(&m_deviceInfos[j]) == config.serialNumber) {
                        selectedIndex = j;
                        break;
                    }
                }
            }

            if (selectedIndex < 0 && i < m_deviceInfos.size() && !used[i]) {
                selectedIndex = i;
            }

            if (selectedIndex < 0) {
                for (int j = 0; j < m_deviceInfos.size(); ++j) {
                    if (!used[j]) {
                        selectedIndex = j;
                        break;
                    }
                }
            }

            if (selectedIndex >= 0) {
                used[selectedIndex] = true;
                result.push_back(m_deviceInfos[selectedIndex]);
            }
        }
    }

    return result;
}

void MultiCameraController::setControllerState(MultiCameraSyncState state, const QString& errorString)
{
    {
        QMutexLocker locker(&m_mutex);
        m_controllerState = state;
        m_lastError = errorString;
    }

    if (!errorString.isEmpty()) {
        emit errorOccurred(errorString);
    }
    emit statusChanged();
}

bool MultiCameraController::setControllerError(const QString& errorString)
{
    setControllerState(MultiCameraSyncState::Error, errorString);
    return false;
}

bool MultiCameraController::allUnitsOpen() const
{
    if (m_units.isEmpty()) {
        return false;
    }

    for (const MultiCameraUnit *unit : m_units) {
        if (!unit->isOpen()) {
            return false;
        }
    }

    return true;
}

bool MultiCameraController::allUnitsGrabbing() const
{
    if (m_units.isEmpty()) {
        return false;
    }

    for (const MultiCameraUnit *unit : m_units) {
        if (!unit->isGrabbing()) {
            return false;
        }
    }

    return true;
}

int MultiCameraController::expectedCameraCount() const
{
    return m_syncManager.expectedCameraCount();
}

int MultiCameraController::remainingTimeoutMs(const QElapsedTimer& timer, int totalTimeoutMs) const
{
    return qMax(0, totalTimeoutMs - static_cast<int>(timer.elapsed()));
}
