#pragma once

#include "TrackingTypes.h"

#include <functional>

class DetectionSession
{
public:
    using RunIdFactory = std::function<QString()>;

    explicit DetectionSession(
        const RunIdFactory &runIdFactory = RunIdFactory());

    QString begin();
    bool isActive() const;
    QString runId() const;
    quint64 completedProductCount() const;

    DetectionCompletion complete(
        const cv::Mat &image,
        const DetectionResult &result,
        quint64 frameNumber = 0,
        int cameraIndex = 0,
        const QDateTime &timestampUtc = QDateTime());

private:
    RunIdFactory m_runIdFactory;
    QString m_runId;
    quint64 m_productSequence = 0;
};
