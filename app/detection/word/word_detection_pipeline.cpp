#include "word_detection_pipeline.h"

#include <QRegularExpression>

namespace {

QStringList parseTargetUnits(const QString &targetText)
{
    QStringList units;
    const QRegularExpression expression(
                R"(([\d[A-Za-z\x{4e00}-\x{9fa5}]\(\d+\))|(\d)|([A-Za-z])|([\x{4e00}-\x{9fa5}]))");
    QRegularExpressionMatchIterator matches =
            expression.globalMatch(targetText);

    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        QString unit;
        if (!match.captured(1).isEmpty()) {
            unit = match.captured(1);
        } else if (!match.captured(2).isEmpty()) {
            unit = match.captured(2);
        } else if (!match.captured(3).isEmpty()) {
            unit = match.captured(3);
        } else if (!match.captured(4).isEmpty()) {
            unit = match.captured(4);
        }

        if (!unit.isEmpty()) {
            units.append(unit.toLower());
        }
    }
    return units;
}

} // namespace

WordDetectionResult WordDetectionPipeline::detect(
        cv::Mat &dateRoi,
        const QString &targetText,
        const CharacterMatchFunction &matchCharacters) const
{
    WordDetectionResult result;
    result.targetUnits = parseTargetUnits(targetText);
    result.targetCharacterCount = result.targetUnits.size();
    if (result.targetCharacterCount == 0 && !targetText.isEmpty()) {
        result.targetCharacterCount = targetText.length();
    }

    if (dateRoi.empty() || !matchCharacters) {
        return result;
    }

    result.detectedCharacterCount = matchCharacters(dateRoi);
    result.isOk =
            result.detectedCharacterCount == result.targetCharacterCount;
    return result;
}
