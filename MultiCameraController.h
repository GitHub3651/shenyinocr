#ifndef MULTICAMERACONTROLLER_H
#define MULTICAMERACONTROLLER_H

#include "IMultiCameraProvider.h"
#include "MultiCameraSyncManager.h"
#include "MultiCameraUnit.h"

#include <QObject>
#include <QMutex>
#include <QVector>

class QElapsedTimer;

class MultiCameraController : public QObject, public IMultiCameraProvider
{
    Q_OBJECT

public:
    explicit MultiCameraController(QObject *parent = nullptr);
    ~MultiCameraController();

    bool openAll() override;
    void closeAll() override;
    bool startAll() override;
    void stopAll() override;
    bool triggerOnce() override;
    bool grabShot(MultiCameraShot& shot, int timeoutMs) override;
    MultiCameraStatus status() const override;

    QVector<CameraDeviceConfig> scanDevices();
    QVector<CameraDeviceConfig> availableDevices() const;

    void setDeviceConfigs(const QVector<CameraDeviceConfig>& configs);
    QVector<CameraDeviceConfig> deviceConfigs() const;

    void setRuntimeConfig(int cameraIndex, const CameraRuntimeConfig& config);
    CameraRuntimeConfig runtimeConfig(int cameraIndex) const;

    void setMaxTimestampDiffUs(qint64 maxDiffUs);
    qint64 maxTimestampDiffUs() const;

    void setRequireEqualFrameId(bool enabled);
    bool requireEqualFrameId() const;

signals:
    void shotReady(const MultiCameraShot& shot);
    void statusChanged();
    void errorOccurred(const QString& errorString);

private slots:
    void onUnitError(int cameraIndex, const QString& errorString);

private:
    static QString serialNumberFromDeviceInfo(const MV_CC_DEVICE_INFO *deviceInfo);
    static QString userDefinedNameFromDeviceInfo(const MV_CC_DEVICE_INFO *deviceInfo);
    static QString modelNameFromDeviceInfo(const MV_CC_DEVICE_INFO *deviceInfo);

    CameraDeviceConfig configFromDeviceInfo(int logicalIndex,
                                            const MV_CC_DEVICE_INFO *deviceInfo) const;
    bool ensureDeviceList();
    QVector<MV_CC_DEVICE_INFO> resolveOpenDeviceInfos();
    void setControllerState(MultiCameraSyncState state, const QString& errorString = QString());
    bool setControllerError(const QString& errorString);
    bool allUnitsOpen() const;
    bool allUnitsGrabbing() const;
    int expectedCameraCount() const;
    int remainingTimeoutMs(const QElapsedTimer& timer, int totalTimeoutMs) const;

private:
    mutable QMutex m_mutex;
    QVector<MultiCameraUnit*> m_units;
    QVector<MV_CC_DEVICE_INFO> m_deviceInfos;
    QVector<CameraDeviceConfig> m_availableDevices;
    QVector<CameraDeviceConfig> m_deviceConfigs;
    MultiCameraSyncManager m_syncManager;
    quint64 m_currentShotId = 0;
    quint64 m_pendingShotId = 0;
    MultiCameraSyncState m_controllerState = MultiCameraSyncState::Idle;
    QString m_lastError;
};

#endif // MULTICAMERACONTROLLER_H
