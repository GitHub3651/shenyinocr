#ifndef DETECTION_WORD_WORD_DETECTION_PIPELINE_H
#define DETECTION_WORD_WORD_DETECTION_PIPELINE_H

#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

#include <functional>

struct WordDetectionResult
{
    QStringList targetUnits;
    int targetCharacterCount = 0;
    int detectedCharacterCount = 0;
    bool isOk = false;
};

class WordDetectionPipeline
{
public:
    typedef std::function<int(cv::Mat &dateRoi)> CharacterMatchFunction;

    WordDetectionResult detect(
            cv::Mat &dateRoi,
            const QString &targetText,
            const CharacterMatchFunction &matchCharacters) const;
};

#endif // DETECTION_WORD_WORD_DETECTION_PIPELINE_H
