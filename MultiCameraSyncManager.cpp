#include "MultiCameraSyncManager.h"

#include <QMutexLocker>
#include <QtGlobal>
#include <limits>

MultiCameraSyncManager::MultiCameraSyncManager(int expectedCameraCount)
    : m_expectedCameraCount(qMax(1, expectedCameraCount))
{
}

void MultiCameraSyncManager::reset()
{
    QMutexLocker locker(&m_mutex);
    m_syncState = MultiCameraSyncState::Idle;
    m_pairCount = 0;
    m_lostCount = 0;
    m_timeoutCount = 0;
    m_lastTimestampDiffUs = 0;
    m_lastError.clear();
}

int MultiCameraSyncManager::expectedCameraCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_expectedCameraCount;
}

void MultiCameraSyncManager::setExpectedCameraCount(int cameraCount)
{
    QMutexLocker locker(&m_mutex);
    m_expectedCameraCount = qMax(1, cameraCount);
}

qint64 MultiCameraSyncManager::maxTimestampDiffUs() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxTimestampDiffUs;
}

void MultiCameraSyncManager::setMaxTimestampDiffUs(qint64 maxDiffUs)
{
    QMutexLocker locker(&m_mutex);
    m_maxTimestampDiffUs = qMax<qint64>(0, maxDiffUs);
}

bool MultiCameraSyncManager::requireEqualFrameId() const
{
    QMutexLocker locker(&m_mutex);
    return m_requireEqualFrameId;
}

void MultiCameraSyncManager::setRequireEqualFrameId(bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_requireEqualFrameId = enabled;
}

bool MultiCameraSyncManager::assembleShot(quint64 shotId,
                                          const QVector<CameraFrame>& frames,
                                          MultiCameraShot& shot)
{
    const QVector<CameraFrame> shotFrames = cloneFrames(frames);
    const qint64 diffUs = timestampDiffUs(shotFrames);
    int expectedCount = 0;
    qint64 maxDiffUs = 0;
    bool requireEqualFrame = true;

    {
        QMutexLocker locker(&m_mutex);
        expectedCount = m_expectedCameraCount;
        maxDiffUs = m_maxTimestampDiffUs;
        requireEqualFrame = m_requireEqualFrameId;
    }

    shot = MultiCameraShot();
    shot.shotId = shotId;
    shot.expectedCameraCount = expectedCount;
    shot.frames = shotFrames;
    shot.timestampDiffUs = diffUs;

    if (shotFrames.size() != expectedCount) {
        recordFailure(shotId,
                      shotFrames,
                      MultiCameraSyncState::Incomplete,
                      QStringLiteral("Incomplete shot: expected %1 frames, got %2.")
                              .arg(expectedCount)
                              .arg(shotFrames.size()),
                      shot);
        return false;
    }

    for (const CameraFrame& frame : shotFrames) {
        if (!frame.hasImage()) {
            recordFailure(shotId,
                          shotFrames,
                          MultiCameraSyncState::Incomplete,
                          frame.errorString.isEmpty()
                                  ? QStringLiteral("Incomplete shot: invalid camera frame.")
                                  : frame.errorString,
                          shot);
            return false;
        }

        if (frame.shotId != shotId) {
            recordFailure(shotId,
                          shotFrames,
                          MultiCameraSyncState::FrameIdMismatch,
                          QStringLiteral("Frame shotId mismatch: expected %1, got %2 from camera %3.")
                                  .arg(shotId)
                                  .arg(frame.shotId)
                                  .arg(frame.cameraIndex),
                          shot);
            return false;
        }
    }

    if (requireEqualFrame && hasFrameIdMismatch(shotFrames)) {
        recordFailure(shotId,
                      shotFrames,
                      MultiCameraSyncState::FrameIdMismatch,
                      QStringLiteral("FrameId mismatch between cameras."),
                      shot);
        return false;
    }

    if (maxDiffUs > 0 && diffUs > maxDiffUs) {
        recordFailure(shotId,
                      shotFrames,
                      MultiCameraSyncState::TimestampDiffTooLarge,
                      QStringLiteral("Timestamp diff too large: %1 us, limit %2 us.")
                              .arg(diffUs)
                              .arg(maxDiffUs),
                      shot);
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_syncState = MultiCameraSyncState::Complete;
        m_pairCount++;
        m_lastTimestampDiffUs = diffUs;
        m_lastError.clear();
    }

    shot.syncState = MultiCameraSyncState::Complete;
    shot.errorString.clear();
    return true;
}

void MultiCameraSyncManager::recordFailure(quint64 shotId,
                                           const QVector<CameraFrame>& frames,
                                           MultiCameraSyncState state,
                                           const QString& errorString,
                                           MultiCameraShot& shot)
{
    const QVector<CameraFrame> shotFrames = cloneFrames(frames);
    const qint64 diffUs = timestampDiffUs(shotFrames);
    int expectedCount = 0;

    {
        QMutexLocker locker(&m_mutex);
        expectedCount = m_expectedCameraCount;
    }

    shot = MultiCameraShot();
    shot.shotId = shotId;
    shot.expectedCameraCount = expectedCount;
    shot.frames = shotFrames;
    shot.timestampDiffUs = diffUs;
    shot.syncState = state;
    shot.errorString = errorString;

    updateFailureStats(state, diffUs, errorString);
}

void MultiCameraSyncManager::recordTimeout(quint64 shotId,
                                           const QVector<CameraFrame>& frames,
                                           const QString& errorString,
                                           MultiCameraShot& shot)
{
    recordFailure(shotId, frames, MultiCameraSyncState::Timeout, errorString, shot);
}

MultiCameraStatus MultiCameraSyncManager::status() const
{
    QMutexLocker locker(&m_mutex);
    MultiCameraStatus status;
    status.syncState = m_syncState;
    status.pairCount = m_pairCount;
    status.lostCount = m_lostCount;
    status.timeoutCount = m_timeoutCount;
    status.lastTimestampDiffUs = m_lastTimestampDiffUs;
    status.lastError = m_lastError;
    return status;
}

CameraFrame MultiCameraSyncManager::cloneFrame(const CameraFrame& frame) const
{
    CameraFrame cloned = frame;
    cloned.image = frame.image.clone();
    return cloned;
}

QVector<CameraFrame> MultiCameraSyncManager::cloneFrames(const QVector<CameraFrame>& frames) const
{
    QVector<CameraFrame> cloned;
    cloned.reserve(frames.size());
    for (const CameraFrame& frame : frames) {
        cloned.push_back(cloneFrame(frame));
    }
    return cloned;
}

qint64 MultiCameraSyncManager::timestampDiffUs(const QVector<CameraFrame>& frames) const
{
    qint64 minTimestamp = std::numeric_limits<qint64>::max();
    qint64 maxTimestamp = std::numeric_limits<qint64>::min();
    bool hasTimestamp = false;

    for (const CameraFrame& frame : frames) {
        if (frame.timestampUs <= 0) {
            continue;
        }
        minTimestamp = qMin(minTimestamp, frame.timestampUs);
        maxTimestamp = qMax(maxTimestamp, frame.timestampUs);
        hasTimestamp = true;
    }

    if (!hasTimestamp) {
        return 0;
    }

    return maxTimestamp - minTimestamp;
}

bool MultiCameraSyncManager::hasFrameIdMismatch(const QVector<CameraFrame>& frames) const
{
    if (frames.isEmpty()) {
        return false;
    }

    const quint64 firstFrameId = frames.first().frameId;
    if (firstFrameId == 0) {
        return true;
    }

    for (const CameraFrame& frame : frames) {
        if (frame.frameId == 0 || frame.frameId != firstFrameId) {
            return true;
        }
    }

    return false;
}

void MultiCameraSyncManager::updateFailureStats(MultiCameraSyncState state,
                                                qint64 timestampDiffUs,
                                                const QString& errorString)
{
    QMutexLocker locker(&m_mutex);
    m_syncState = state;
    m_lostCount++;
    if (state == MultiCameraSyncState::Timeout) {
        m_timeoutCount++;
    }
    m_lastTimestampDiffUs = timestampDiffUs;
    m_lastError = errorString;
}
