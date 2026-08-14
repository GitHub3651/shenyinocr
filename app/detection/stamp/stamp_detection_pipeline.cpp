#include "stamp_detection_pipeline.h"

#include "detection/common/detection_roi_geometry.h"

#include <QRegularExpression>

#include <chrono>

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

StampDetectionWorkOutput StampDetectionPipeline::detect(
        const DetectionWorkItem &item,
        const QString &targetText,
        const TemplateMatchPreparedTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes,
        int thresholdPercent,
        const OverlapDetectionFunction &detectOverlap) const
{
    StampDetectionWorkOutput output;
    output.pose = item.pose;
    output.hasOverlapDetection = static_cast<bool>(detectOverlap);
    output.detectionResult.modeId = QStringLiteral("stamp_detection");
    output.detectionResult.status = DetectionStatus::Cancelled;
    output.detectionResult.diagnostic =
            QStringLiteral("Invalid stamp detection work item");
    if (!item.isValid() || !item.hasPose) {
        return output;
    }

    const std::chrono::high_resolution_clock::time_point start =
            std::chrono::high_resolution_clock::now();
    const OrientedDateRoi oriented =
            DetectionRoiGeometry::prepareOrientedDateRoi(
                item.frame->originalImage,
                item.pose,
                20);
    if (!oriented.valid) {
        output.detectionResult.diagnostic =
                QStringLiteral("Invalid stamp date ROI");
        return output;
    }
    output.roiValid = true;

    const CharacterTemplateMatchResult matchResult =
            CharacterTemplateMatcher::match(
                oriented.croppedImage,
                preparedTemplates,
                templateTargetIndexes,
                thresholdPercent);
    cv::Mat dateRoi = oriented.croppedImage;
    output.stampResult = detect(
                dateRoi,
                item.frame->originalImage,
                item.pose.datePoly,
                targetText,
                [matchResult](cv::Mat &) {
        return matchResult.detectedCount;
    },
                detectOverlap);

    DetectionResult &result = output.detectionResult;
    result.status = DetectionStatus::Completed;
    result.verdict = output.stampResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    if (!output.stampResult.characterIsOk
            && output.stampResult.overlapIsOk) {
        result.diagnostic = QStringLiteral(
                    "\u55b7\u7801\u4e0d\u5408\u683c");
    } else if (output.stampResult.characterIsOk
               && !output.stampResult.overlapIsOk) {
        result.diagnostic = QStringLiteral(
                    "\u94a2\u5370\u91cd\u53e0");
    } else if (!output.stampResult.isOk) {
        result.diagnostic = QStringLiteral(
                    "\u55b7\u7801\u4e0e\u94a2\u5370"
                    "\u5747\u4e0d\u5408\u683c");
    } else {
        result.diagnostic = QStringLiteral(
                    "\u55b7\u7801\u4e0e\u94a2\u5370"
                    "\u5747\u5408\u683c");
    }
    result.elapsedMs = static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - start)
                .count());

    const auto appendPolygon = [&result](
            const QString &role,
            const std::vector<cv::Point> &points,
            double score) {
        if (points.empty()) {
            return;
        }
        DetectionOverlayPolygon polygon;
        polygon.role = role;
        polygon.points = points;
        polygon.score = score;
        result.overlay.polygons.push_back(polygon);
    };
    appendPolygon(QStringLiteral("tracking"),
                  item.pose.trackingPoly,
                  item.pose.score);
    appendPolygon(QStringLiteral("date"),
                  item.pose.datePoly,
                  0.0);
    const std::vector<DetectionOverlayPolygon> characterPolygons =
            DetectionRoiGeometry::mapCharacterMatchesToOverlay(
                matchResult.matches,
                oriented,
                item.frame->originalImage.size());
    result.overlay.polygons.insert(result.overlay.polygons.end(),
                                   characterPolygons.begin(),
                                   characterPolygons.end());
    appendPolygon(QStringLiteral("stamp"),
                  output.stampResult.finalStampPoly,
                  0.0);
    return output;
}
