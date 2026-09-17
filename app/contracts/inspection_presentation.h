#pragma once

#include <QImage>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

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

struct InspectionPresentation
{
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
        return !image.isNull();
    }
};

Q_DECLARE_METATYPE(InspectionPresentation)
