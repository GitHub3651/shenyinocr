#pragma once

#include "TrackingTypes.h"

#include <functional>
#include <map>

class DetectionSession
{
public:
    using RunIdFactory = std::function<QString()>;

    explicit DetectionSession(
        const RunIdFactory &runIdFactory = RunIdFactory());

    QString begin();
    bool isActive() const;
    QString runId() const;
    quint64 acceptedProductCount() const;
    quint64 completedProductCount() const;

    std::shared_ptr<const FrameData> acceptFrame(
        const cv::Mat &image,
        quint64 frameNumber = 0,
        int cameraIndex = 0,
        const QDateTime &timestampUtc = QDateTime());

    DetectionCompletion complete(
        const std::shared_ptr<const FrameData> &frame,
        const DetectionResult &result);

    DetectionCompletion complete(
        const cv::Mat &image,
        const DetectionResult &result,
        quint64 frameNumber = 0,
        int cameraIndex = 0,
        const QDateTime &timestampUtc = QDateTime());

private:
    RunIdFactory m_runIdFactory;
    QString m_runId;
    quint64 m_acceptedProductSequence = 0;
    quint64 m_completedProductCount = 0;
    quint64 m_lastCompletedProductSequence = 0;
    std::map<quint64, std::weak_ptr<const FrameData>> m_acceptedFrames;
};
