#ifndef MULTICAMERAUNIT_H
#define MULTICAMERAUNIT_H

#include "MultiCameraTypes.h"
#include "cmvcamera.h"

#include <QObject>
#include <QMutex>
#include <QString>
#include <memory>

class MultiCameraUnit : public QObject
{
    Q_OBJECT

public:
    explicit MultiCameraUnit(int cameraIndex, QObject *parent = nullptr);
    ~MultiCameraUnit();

    MultiCameraUnit(const MultiCameraUnit&) = delete;
    MultiCameraUnit& operator=(const MultiCameraUnit&) = delete;

    int cameraIndex() const;
    CameraDeviceConfig deviceConfig() const;
    CameraRuntimeConfig runtimeConfig() const;
    bool isOpen() const;
    bool isGrabbing() const;
    bool isDeviceConnected() const;
    quint64 frameId() const;
    QString lastError() const;
    CameraFrame lastFrame() const;

    void setDeviceConfig(const CameraDeviceConfig& config);
    void setRuntimeConfig(const CameraRuntimeConfig& config);

    bool open(MV_CC_DEVICE_INFO *deviceInfo,
              const CameraDeviceConfig& deviceConfig = CameraDeviceConfig());
    void close();

    bool start();
    void stop();

    bool setExposure(double exposureUs);
    bool setGain(double gain);
    bool setTriggerMode(MultiCameraTriggerMode triggerMode);
    bool applyRuntimeConfig(const CameraRuntimeConfig& config);

    bool triggerOnce(quint64 shotId);
    bool grabFrame(CameraFrame& frame, quint64 shotId, int timeoutMs);
    void clearLastFrame();

signals:
    void frameReady(const CameraFrame& frame);
    void stateChanged();
    void errorOccurred(int cameraIndex, const QString& errorString);

private:
    QString sdkErrorString(const QString& operation, int ret) const;
    bool setError(const QString& errorString);
    void fillDeviceConfigDefaults();
    static QString serialNumberFromDeviceInfo(const MV_CC_DEVICE_INFO *deviceInfo);
    bool copyLatestImage(CMvCamera *camera, CameraFrame& frame, quint64 shotId, quint64 minimumFrameSeq);

private:
    mutable QMutex m_mutex;
    std::unique_ptr<CMvCamera> m_camera;
    CameraDeviceConfig m_deviceConfig;
    CameraRuntimeConfig m_runtimeConfig;
    CameraFrame m_lastFrame;
    QString m_lastError;
    bool m_opened = false;
    bool m_grabbing = false;
    bool m_callbackRegistered = false;
    quint64 m_lastFrameSeq = 0;
    quint64 m_waitBaseFrameSeq = 0;
    quint64 m_lastShotId = 0;
};

#endif // MULTICAMERAUNIT_H
