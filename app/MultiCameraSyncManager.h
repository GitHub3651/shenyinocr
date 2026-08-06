#ifndef MULTICAMERASYNCMANAGER_H
#define MULTICAMERASYNCMANAGER_H

#include "MultiCameraTypes.h"

#include <QMutex>

class MultiCameraSyncManager
{
public:
    explicit MultiCameraSyncManager(int expectedCameraCount = 2);

    void reset();

    int expectedCameraCount() const;
    void setExpectedCameraCount(int cameraCount);

    qint64 maxTimestampDiffUs() const;
    void setMaxTimestampDiffUs(qint64 maxDiffUs);

    bool requireEqualFrameId() const;
    void setRequireEqualFrameId(bool enabled);

    bool assembleShot(quint64 shotId,
                      const QVector<CameraFrame>& frames,
                      MultiCameraShot& shot);
    void recordFailure(quint64 shotId,
                       const QVector<CameraFrame>& frames,
                       MultiCameraSyncState state,
                       const QString& errorString,
                       MultiCameraShot& shot);
    void recordTimeout(quint64 shotId,
                       const QVector<CameraFrame>& frames,
                       const QString& errorString,
                       MultiCameraShot& shot);

    MultiCameraStatus status() const;

private:
    CameraFrame cloneFrame(const CameraFrame& frame) const;
    QVector<CameraFrame> cloneFrames(const QVector<CameraFrame>& frames) const;
    qint64 timestampDiffUs(const QVector<CameraFrame>& frames) const;
    bool hasFrameIdMismatch(const QVector<CameraFrame>& frames) const;
    void updateFailureStats(MultiCameraSyncState state, qint64 timestampDiffUs, const QString& errorString);

private:
    mutable QMutex m_mutex;
    int m_expectedCameraCount = 2;
    qint64 m_maxTimestampDiffUs = 5000;
    bool m_requireEqualFrameId = true;
    MultiCameraSyncState m_syncState = MultiCameraSyncState::Idle;
    quint64 m_pairCount = 0;
    quint64 m_lostCount = 0;
    quint64 m_timeoutCount = 0;
    qint64 m_lastTimestampDiffUs = 0;
    QString m_lastError;
};

#endif // MULTICAMERASYNCMANAGER_H
