// 文件作用：本文件用于执行字库模板模式的定位、字符分割、模板匹配和结果生成。
// 主要职责：执行字库模板模式的定位、字符分割、模板匹配和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "word_detection_pipeline.h"

#include "detection/common/detection_roi_geometry.h"

#include <QRegularExpression>

#include <chrono>

namespace {

// 函数说明：parseTargetUnits 函数校验、转换或恢复对应数据。
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

// 函数说明：detect 函数执行对应事件或业务处理。
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

// 函数说明：detect 函数执行对应事件或业务处理。
WordDetectionWorkOutput WordDetectionPipeline::detect(
        const DetectionWorkItem &item,
        const QString &targetText,
        const QString &templateName,
        const PreparedCharacterTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes,
        int thresholdPercent) const
{
    if (!item.isValid() || !item.hasPose || !item.pose.valid) {
        return detectPreparedDateRoi(
                    item,
                    OrientedDateRoi(),
                    targetText,
                    templateName,
                    preparedTemplates,
                    templateTargetIndexes,
                    thresholdPercent);
    }

    const std::chrono::high_resolution_clock::time_point prepareStart =
            std::chrono::high_resolution_clock::now();
    const OrientedDateRoi oriented =
            DetectionRoiGeometry::prepareOrientedDateRoi(
                item.frame->originalImage,
                item.pose,
                20);
    const double prepareElapsedMs = static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now()
                    - prepareStart).count());
    WordDetectionWorkOutput output = detectPreparedDateRoi(
                item,
                oriented,
                targetText,
                templateName,
                preparedTemplates,
                templateTargetIndexes,
                thresholdPercent);
    if (output.detectionResult.status == DetectionStatus::Completed) {
        output.detectionResult.elapsedMs += prepareElapsedMs;
    }
    return output;
}

// 函数说明：detectPreparedDateRoi 函数执行对应事件或业务处理。
WordDetectionWorkOutput WordDetectionPipeline::detectPreparedDateRoi(
        const DetectionWorkItem &item,
        const OrientedDateRoi &oriented,
        const QString &targetText,
        const QString &templateName,
        const PreparedCharacterTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes,
        int thresholdPercent) const
{
    WordDetectionWorkOutput output;
    output.pose = item.pose;
    output.templateName = templateName;
    DetectionResult &result = output.detectionResult;
    result.modeId = QStringLiteral("word_detection");
    result.status = DetectionStatus::Cancelled;
    result.diagnostic = QStringLiteral("Invalid word detection work item");
    if (!item.isValid() || !item.hasPose) {
        return output;
    }

    if (!item.pose.valid) {
        result.status = DetectionStatus::Completed;
        result.verdict = AlgorithmVerdict::Ng;
        result.diagnostic = QStringLiteral(
                    "\u672a\u627e\u5230\u5b57\u5e93"
                    "\u5b9a\u4f4d\u533a\u57df");
        result.elapsedMs = item.pose.trackingElapsedMs;
        output.reason = result.diagnostic;
        return output;
    }

    const std::chrono::high_resolution_clock::time_point start =
            std::chrono::high_resolution_clock::now();
    if (!oriented.valid) {
        result.diagnostic = QStringLiteral(
                    "\u65e5\u671fROI\u65e0\u6548\u6216"
                    "\u8d85\u51fa\u539f\u56fe\u8303\u56f4");
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
                targetText,
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
                    "\u8bc6\u522b\u6570\u91cf\u7b49\u4e8e"
                    "\u76ee\u6807\u6570\u91cf");
    } else if (output.wordResult.detectedCharacterCount
               < output.wordResult.targetCharacterCount) {
        output.reason = QStringLiteral(
                    "\u8bc6\u522b\u6570\u91cf\u5c11\u4e8e"
                    "\u76ee\u6807\u6570\u91cf\uff0c"
                    "\u5c11%1\u4e2a")
                .arg(output.wordResult.targetCharacterCount
                     - output.wordResult.detectedCharacterCount);
    } else {
        output.reason = QStringLiteral(
                    "\u8bc6\u522b\u6570\u91cf\u591a\u4e8e"
                    "\u76ee\u6807\u6570\u91cf\uff0c"
                    "\u591a%1\u4e2a")
                .arg(output.wordResult.detectedCharacterCount
                     - output.wordResult.targetCharacterCount);
    }
    if (!output.missingUnits.isEmpty()) {
        output.reason += QStringLiteral(
                    "\uff1b\u672a\u5339\u914d\u76ee\u6807=%1")
                .arg(output.missingUnits.join(QStringLiteral(", ")));
    }

    result.status = DetectionStatus::Completed;
    result.verdict = output.wordResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    result.recognizedText = output.detectedUnits.join(QString());
    result.diagnostic = output.reason;
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
    return output;
}
