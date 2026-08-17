#pragma once

#include "TrackingTypes.h"

#include <QImage>
#include <QString>

enum class DetectionVerdictViewStyle
{
    Correct,
    Error
};

struct DetectionResultStatistics
{
    int totalCount = 0;
    int ngCount = 0;

    double passRatePercent() const
    {
        return totalCount > 0
                ? (1.0 - static_cast<double>(ngCount) / totalCount) * 100.0
                : 0.0;
    }
};

struct DetectionAbnormalStatistics
{
    quint64 systemFaultCount = 0;
    quint64 unconfirmedProductCount = 0;
    quint64 postFaultDroppedFrameCount = 0;
};

// Complete read-only product presentation. The UI applies this object as one
// unit so image, verdict, text, template, statistics and timing cannot drift
// between different ProductKeys.
struct InspectionPresentation
{
    ProductKey productKey;
    QImage image;
    DetectionVerdictViewStyle verdictStyle =
            DetectionVerdictViewStyle::Error;
    QString recognitionText;
    bool updatesTemplateName = false;
    QString templateName;
    DetectionResultStatistics statistics;
    QString elapsedText;

    bool isValid() const
    {
        return productKey.isValid() && !image.isNull();
    }
};
