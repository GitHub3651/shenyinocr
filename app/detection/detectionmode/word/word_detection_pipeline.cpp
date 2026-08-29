// 文件作用：本文件用于执行字库模板模式的定位、字符分割、模板匹配和结果生成。
// 主要职责：执行字库模板模式的定位、字符分割、模板匹配和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "word_detection_pipeline.h"
#include "contracts/detection_mode.h"

#include "detection/common/detection_roi_geometry.h"

#include <stdexcept>

// 函数说明：detect 函数执行对应事件或业务处理。
WordDetectionResult WordDetectionPipeline::detect(
        cv::Mat &dateRoi,
        const QStringList &targetUnits,
        const CharacterMatchFunction &matchCharacters) const
{
    WordDetectionResult result;
    result.targetUnits = targetUnits;
    result.targetCharacterCount = result.targetUnits.size();

    if (dateRoi.empty() || !matchCharacters) {
        return result;
    }

    result.detectedCharacterCount = matchCharacters(dateRoi);
    result.isOk =
            result.detectedCharacterCount == result.targetCharacterCount;
    return result;
}

// 函数说明：detect 函数执行对应事件或业务处理。
WordDetectionWorkOutput WordDetectionPipeline::detect(
        const DetectionWorkItem &item,
        const QStringList &targetUnits,
        const QString &templateName,
        const PreparedCharacterTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes,
        int thresholdPercent) const
{
    if (!item.isValid() || !item.hasPose) {
        throw std::invalid_argument("Invalid word detection work item");
    }
    if (!item.pose.valid) {
        return detectPreparedDateRoi(
                    item,
                    OrientedDateRoi(),
                    targetUnits,
                    templateName,
                    preparedTemplates,
                    templateTargetIndexes,
                    thresholdPercent);
    }

    const OrientedDateRoi oriented =
            DetectionRoiGeometry::prepareOrientedDateRoi(
                item.frame->originalImage,
                item.pose,
                20);
    return detectPreparedDateRoi(
                item,
                oriented,
                targetUnits,
                templateName,
                preparedTemplates,
                templateTargetIndexes,
                thresholdPercent);
}

// 函数说明：detectPreparedDateRoi 函数执行对应事件或业务处理。
WordDetectionWorkOutput WordDetectionPipeline::detectPreparedDateRoi(
        const DetectionWorkItem &item,
        const OrientedDateRoi &oriented,
        const QStringList &targetUnits,
        const QString &templateName,
        const PreparedCharacterTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes,
        int thresholdPercent) const
{
    WordDetectionWorkOutput output;
    output.pose = item.pose;
    output.templateName = templateName;
    DetectionResult &result = output.detectionResult;
    result.modeId = detectionModeUiId(DetectionMode::Word);
    result.diagnostic = QStringLiteral("Invalid word detection work item");
    if (!item.isValid() || !item.hasPose) {
        throw std::invalid_argument("Invalid word detection work item");
    }

    if (!item.pose.valid) {
        result.verdict = AlgorithmVerdict::Ng;
        result.diagnostic = QStringLiteral(
                    "未找到字库"
                    "定位区域");
        output.reason = result.diagnostic;
        return output;
    }

    if (!oriented.valid) {
        result.verdict = AlgorithmVerdict::Ng;
        result.diagnostic = QStringLiteral(
                    "日期ROI无效或"
                    "超出原图范围");
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
    output.wordResult = detect(
                dateRoi,
                targetUnits,
                [matchResult](cv::Mat &) {
        return matchResult.detectedCount;
    });

    output.detectedUnits.reserve(
                static_cast<int>(matchResult.matches.size()));
    output.matchDetails.reserve(
                static_cast<int>(matchResult.matches.size()));
    for (int i = 0;
         i < static_cast<int>(matchResult.matches.size());
         ++i) {
        const std::tuple<cv::Rect, double, size_t> &match =
                matchResult.matches[static_cast<size_t>(i)];
        const cv::Rect rect = std::get<0>(match);
        const double score = std::get<1>(match);
        const int targetIndex = static_cast<int>(std::get<2>(match));
        const QString unit = output.wordResult.targetUnits.value(
                    targetIndex,
                    QStringLiteral("#%1").arg(targetIndex));
        output.detectedUnits.append(unit);
        output.matchDetails.append(
                    QStringLiteral("%1:%2 score=%3 rect=(%4,%5,%6,%7) targetIndex=%8")
                    .arg(i + 1)
                    .arg(unit)
                    .arg(score, 0, 'f', 3)
                    .arg(rect.x)
                    .arg(rect.y)
                    .arg(rect.width)
                    .arg(rect.height)
                    .arg(targetIndex));
    }
    for (int i = 0; i < output.wordResult.targetUnits.size(); ++i) {
        bool found = false;
        for (const std::tuple<cv::Rect, double, size_t> &match :
             matchResult.matches) {
            if (static_cast<int>(std::get<2>(match)) == i) {
                found = true;
                break;
            }
        }
        if (!found) {
            output.missingUnits.append(
                        QStringLiteral("%1:%2")
                        .arg(i + 1)
                        .arg(output.wordResult.targetUnits.at(i)));
        }
    }

    if (output.wordResult.isOk) {
        output.reason = QStringLiteral(
                    "识别数量等于"
                    "目标数量");
    } else if (output.wordResult.detectedCharacterCount
               < output.wordResult.targetCharacterCount) {
        output.reason = QStringLiteral(
                    "识别数量少于"
                    "目标数量，"
                    "少%1个")
                .arg(output.wordResult.targetCharacterCount
                     - output.wordResult.detectedCharacterCount);
    } else {
        output.reason = QStringLiteral(
                    "识别数量多于"
                    "目标数量，"
                    "多%1个")
                .arg(output.wordResult.detectedCharacterCount
                     - output.wordResult.targetCharacterCount);
    }
    if (!output.missingUnits.isEmpty()) {
        output.reason += QStringLiteral(
                    "；未匹配目标=%1")
                .arg(output.missingUnits.join(QStringLiteral(", ")));
    }

    result.verdict = output.wordResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    result.recognizedText = output.detectedUnits.join(QString());
    result.diagnostic = output.reason;

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
    return output;
}
