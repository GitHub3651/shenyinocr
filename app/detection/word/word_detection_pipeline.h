#ifndef DETECTION_WORD_WORD_DETECTION_PIPELINE_H
#define DETECTION_WORD_WORD_DETECTION_PIPELINE_H

#include "TrackingTypes.h"
#include "detection/common/character_template_matcher.h"

#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

#include <functional>
#include <vector>

struct WordDetectionResult
{
    QStringList targetUnits;
    int targetCharacterCount = 0;
    int detectedCharacterCount = 0;
    bool isOk = false;
};

struct WordDetectionWorkOutput
{
    DetectionResult detectionResult;
    WordDetectionResult wordResult;
    DetectionPose pose;
    QString templateName;
    QStringList detectedUnits;
    QStringList matchDetails;
    QStringList missingUnits;
    QString reason;
    bool roiValid = false;
};

class WordDetectionPipeline
{
public:
    typedef std::function<int(cv::Mat &dateRoi)> CharacterMatchFunction;

    WordDetectionResult detect(
            cv::Mat &dateRoi,
            const QString &targetText,
            const CharacterMatchFunction &matchCharacters) const;

    WordDetectionWorkOutput detect(
            const DetectionWorkItem &item,
            const QString &targetText,
            const QString &templateName,
            const TemplateMatchPreparedTemplates &preparedTemplates,
            const std::vector<int> &templateTargetIndexes,
            int thresholdPercent) const;

    WordDetectionWorkOutput detectPreparedDateRoi(
            const DetectionWorkItem &item,
            const OrientedDateRoi &orientedDateRoi,
            const QString &targetText,
            const QString &templateName,
            const TemplateMatchPreparedTemplates &preparedTemplates,
            const std::vector<int> &templateTargetIndexes,
            int thresholdPercent) const;
};

#endif // DETECTION_WORD_WORD_DETECTION_PIPELINE_H
