#include "stamp_detection_pipeline.h"
#include "contracts/detection_mode.h"

#include "detection/common/detection_roi_geometry.h"

#include <stdexcept>

StampDetectionResult StampDetectionPipeline::detect(
        cv::Mat &dateRoi,
        const cv::Mat &sourceImage,
        const std::vector<cv::Point> &datePoly,
        const QStringList &targetUnits,
        const CharacterMatchFunction &matchCharacters,
        const OverlapDetectionFunction &detectOverlap) const
{
    StampDetectionResult result;
    result.targetCharacterCount = targetUnits.size();
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
        const QStringList &targetUnits,
        const PreparedCharacterTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes,
        int thresholdPercent,
        const OverlapDetectionFunction &detectOverlap) const
{
    StampDetectionWorkOutput output;
    output.pose = item.pose;
    output.hasOverlapDetection = static_cast<bool>(detectOverlap);
    output.detectionResult.modeId = detectionModeUiId(DetectionMode::Stamp);
    output.detectionResult.diagnostic =
            QStringLiteral("Invalid stamp detection work item");
    if (!item.isValid() || !item.hasPose) {
        throw std::invalid_argument("Invalid stamp detection work item");
    }

    if (!item.pose.valid) {
        output.detectionResult.verdict = AlgorithmVerdict::Ng;
        output.detectionResult.diagnostic = QStringLiteral("未找到钢印定位区域");
        return output;
    }

    const OrientedDateRoi oriented =
            DetectionRoiGeometry::prepareOrientedDateRoi(
                item.frame->originalImage,
                item.pose,
                20);
    if (!oriented.valid) {
        output.detectionResult.verdict = AlgorithmVerdict::Ng;
        output.detectionResult.diagnostic =
                QStringLiteral("Invalid stamp date ROI");
        return output;
    }
    output.roiValid = true;

    const CharacterMatchResult matchResult =
            CharacterGlyphMatcher::match(
                oriented.croppedImage,
                preparedTemplates,
                templateTargetIndexes,
                thresholdPercent);
    cv::Mat dateRoi = oriented.croppedImage;
    output.stampResult = detect(
                dateRoi,
                item.frame->originalImage,
                item.pose.datePoly,
                targetUnits,
                [matchResult](cv::Mat &) {
        return matchResult.detectedCount;
    },
                detectOverlap);

    DetectionResult &result = output.detectionResult;
    result.verdict = output.stampResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    if (!output.stampResult.characterIsOk
            && output.stampResult.overlapIsOk) {
        result.diagnostic = QStringLiteral(
                    "喷码不合格");
    } else if (output.stampResult.characterIsOk
               && !output.stampResult.overlapIsOk) {
        result.diagnostic = QStringLiteral(
                    "钢印重叠");
    } else if (!output.stampResult.isOk) {
        result.diagnostic = QStringLiteral(
                    "喷码与钢印"
                    "均不合格");
    } else {
        result.diagnostic = QStringLiteral(
                    "喷码与钢印"
                    "均合格");
    }
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
