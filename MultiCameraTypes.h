#ifndef MULTICAMERATYPES_H
#define MULTICAMERATYPES_H

#include <QRect>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <opencv2/core.hpp>

enum class MultiCameraTriggerMode
{
    Continuous = 0,
    Software,
    Hardware
};

enum class MultiCameraSyncState
{
    Idle = 0,
    Waiting,
    Complete,
    Incomplete,
    Timeout,
    FrameIdMismatch,
    TimestampDiffTooLarge,
    Error
};

struct CameraDeviceConfig
{
    int cameraIndex = -1;
    bool enabled = true;
    QString displayName;
    QString serialNumber;
    QString userDefinedName;
};

struct CameraRuntimeConfig
{
    double exposureUs = 0.0;
    double gain = 0.0;
    MultiCameraTriggerMode triggerMode = MultiCameraTriggerMode::Software;
    QRect roi;
    QString templatePath;
    QString algorithmConfigPath;
};

struct CameraFrame
{
    int cameraIndex = -1;
    QString serialNumber;
    quint64 shotId = 0;
    quint64 frameId = 0;
    qint64 timestampUs = 0;
    cv::Mat image;
    bool valid = false;
    QString errorString;

    bool hasImage() const
    {
        return valid && !image.empty();
    }
};

struct MultiCameraShot
{
    quint64 shotId = 0;
    QVector<CameraFrame> frames;
    int expectedCameraCount = 2;
    qint64 timestampDiffUs = 0;
    MultiCameraSyncState syncState = MultiCameraSyncState::Idle;
    QString errorString;

    bool isComplete() const
    {
        if (syncState != MultiCameraSyncState::Complete) {
            return false;
        }

        if (frames.size() != expectedCameraCount) {
            return false;
        }

        for (const CameraFrame& frame : frames) {
            if (!frame.hasImage() || frame.shotId != shotId) {
                return false;
            }
        }

        return true;
    }
};

struct MultiCameraStatus
{
    bool allOpened = false;
    bool allGrabbing = false;
    MultiCameraSyncState syncState = MultiCameraSyncState::Idle;
    QVector<CameraDeviceConfig> devices;
    QVector<QString> cameraErrors;
    quint64 currentShotId = 0;
    quint64 pairCount = 0;
    quint64 lostCount = 0;
    quint64 timeoutCount = 0;
    qint64 lastTimestampDiffUs = 0;
    QString lastError;
};

#endif // MULTICAMERATYPES_H
