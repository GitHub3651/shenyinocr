#include "stamp_detection_pipeline.h"

#include <QRegularExpression>

namespace {

int countTargetCharacters(const QString &targetText)
{
    const QRegularExpression expression(
                R"(([\d[A-Za-z\x{4e00}-\x{9fa5}]\(\d+\))|(\d)|([A-Za-z])|([\x{4e00}-\x{9fa5}]))");
    QRegularExpressionMatchIterator matches =
            expression.globalMatch(targetText);

    int count = 0;
    while (matches.hasNext()) {
        matches.next();
        ++count;
    }
    if (count == 0 && !targetText.isEmpty()) {
        count = targetText.length();
    }
    return count;
}

} // namespace

StampDetectionResult StampDetectionPipeline::detect(
        cv::Mat &dateRoi,
        const cv::Mat &sourceImage,
        const std::vector<cv::Point> &datePoly,
        const QString &targetText,
        const CharacterMatchFunction &matchCharacters,
        const OverlapDetectionFunction &detectOverlap) const
{
    StampDetectionResult result;
    result.targetCharacterCount = countTargetCharacters(targetText);
    if (dateRoi.empty() || sourceImage.empty() || !matchCharacters) {
        return result;
    }

    result.detectedCharacterCount = matchCharacters(dateRoi);
    result.characterIsOk =
            result.detectedCharacterCount == result.targetCharacterCount;

    if (detectOverlap) {
        const StampOverlapResult overlap =
                detectOverlap(sourceImage, datePoly);
        result.overlapIsOk = overlap.isOk;
        result.finalStampPoly = overlap.finalStampPoly;
    }

    result.isOk = result.characterIsOk && result.overlapIsOk;
    return result;
}
