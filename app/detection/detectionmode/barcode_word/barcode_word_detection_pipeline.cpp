// 文件作用：本文件用于执行二维码与三期字符组合模式的定位、解码、字符检查和结果生成。
// 主要职责：执行二维码与三期字符组合模式的定位、解码、字符检查和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "barcode_word_detection_pipeline.h"
#include "contracts/detection_mode.h"

#include "detection/common/detection_roi_geometry.h"
#include "engines/barcode/barcode_decoder.h"

#include <chrono>

// 函数说明：detect 函数执行对应事件或业务处理。
BarcodeWordDetectionResult BarcodeWordDetectionPipeline::detect(
        bool barcodeIsReadable,
        const DateDetectionFunction &detectDate) const
{
    BarcodeWordDetectionResult result;
    result.barcodeIsReadable = barcodeIsReadable;
    if (!result.barcodeIsReadable || !detectDate) {
        return result;
    }

    result.dateDetectionExecuted = true;
    const BarcodeWordDateDetectionResult dateResult = detectDate();
    result.dateResultProduced = dateResult.resultProduced;
    result.dateIsOk = dateResult.isOk;
    result.isOk = result.dateResultProduced && result.dateIsOk;
    return result;
}

// 函数说明：detect 函数执行对应事件或业务处理。
BarcodeWordDetectionWorkOutput BarcodeWordDetectionPipeline::detect(
        const DetectionWorkItem &item,
        const QString &targetText,
        const QString &templateName,
        const PreparedCharacterTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes,
        int thresholdPercent,
        const BarcodeDecodeOptions &decodeOptions,
        const BarcodeWordDecodeStrategyState &decodeStrategy,
        IBarcodeDecoder *decoder) const
{
    BarcodeWordDetectionWorkOutput output;
    output.pose = item.pose;
    output.templateName = templateName;
    output.nextDecodeStrategy = decodeStrategy;
    output.barcodeState = QStringLiteral("未执行");
    output.dateState = QStringLiteral("未执行");

    DetectionResult &result = output.detectionResult;
    result.modeId = detectionModeUiId(DetectionMode::BarcodeWord);
    result.status = DetectionStatus::Cancelled;
    result.diagnostic = QStringLiteral(
                "Invalid barcode-word detection work item");
    if (!item.isValid() || !item.hasPose) {
        return output;
    }

    const std::chrono::high_resolution_clock::time_point start =
            std::chrono::high_resolution_clock::now();
    const auto elapsedMs = [&start, &item]() {
        return item.pose.trackingElapsedMs
                + static_cast<double>(
                    std::chrono::duration_cast<
                        std::chrono::milliseconds>(
                            std::chrono::high_resolution_clock::now()
                            - start).count());
    };
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
    const auto finishNg = [&output, &result, &elapsedMs](
            const QString &reason) {
        output.reason = reason;
        result.status = DetectionStatus::Completed;
        result.verdict = AlgorithmVerdict::Ng;
        result.recognizedText = output.barcode.text;
        result.diagnostic = reason;
        result.elapsedMs = elapsedMs();
    };

    if (!item.pose.valid) {
        finishNg(QStringLiteral(
                     "未找到定位锚点"));
        return output;
    }

    appendPolygon(QStringLiteral("tracking"),
                  item.pose.trackingPoly,
                  item.pose.score);
    appendPolygon(QStringLiteral("barcode"),
                  item.pose.barcodePoly,
                  0.0);
    appendPolygon(QStringLiteral("date"),
                  item.pose.datePoly,
                  0.0);

    if (item.pose.barcodePoly.size() != 4) {
        output.barcode.status = BarcodeReadStatus::InvalidRoi;
        output.barcode.errorReason = QStringLiteral(
                    "Barcode polygon is missing or invalid");
        finishNg(QStringLiteral(
                     "二维码区域配置"
                     "无效或未映射"));
        return output;
    }
    if (item.pose.datePoly.size() < 3) {
        finishNg(QStringLiteral(
                     "日期检测区域配置"
                     "无效或未映射"));
        return output;
    }

    const DetectionRoiGeometry::BarcodeWordOrientedRois oriented =
            DetectionRoiGeometry::prepareBarcodeWordOrientedRois(
                item.frame->originalImage,
                item.pose,
                decodeOptions.roiPaddingPercent,
                20);
    output.barcodeRoiValid = oriented.barcode.valid;
    output.dateRoiValid = oriented.date.valid;
    if (!output.barcodeRoiValid) {
        output.barcode.status = BarcodeReadStatus::InvalidRoi;
        output.barcode.errorReason = QStringLiteral(
                    "Invalid barcode ROI");
        finishNg(QStringLiteral(
                     "二维码区域无效"
                     "或超出图像范围"));
        return output;
    }

    if (!decoder || !decoder->ensureLoaded()) {
        output.barcode.status = BarcodeReadStatus::DecoderUnavailable;
        output.barcode.errorReason = decoder
                ? decoder->lastError()
                : QStringLiteral("Barcode decoder is null");
        output.barcodeState = QStringLiteral(
                    "读码器不可用");
        finishNg(QStringLiteral("BarcodeDecoder.dll不可用"));
        return output;
    }

    int successfulStrategyId = -1;
    unsigned int successfulOptionFlags =
            BarcodeDecodeOptionFlags::None;
    output.barcode = decoder->decode(
                oriented.barcode.grayRoi,
                decodeOptions,
                decodeStrategy.preferredStrategyId,
                decodeStrategy.preferredOptionFlags,
                &successfulStrategyId,
                &successfulOptionFlags);
    if (output.barcode.readable) {
        output.nextDecodeStrategy.preferredStrategyId =
                successfulStrategyId;
        output.nextDecodeStrategy.preferredOptionFlags =
                successfulOptionFlags;
        output.nextDecodeStrategy.consecutiveFailures = 0;
    } else {
        output.nextDecodeStrategy.consecutiveFailures =
                (std::min)(3,
                           decodeStrategy.consecutiveFailures + 1);
        if (output.nextDecodeStrategy.consecutiveFailures >= 3) {
            output.nextDecodeStrategy.preferredStrategyId = -1;
            output.nextDecodeStrategy.preferredOptionFlags =
                    BarcodeDecodeOptionFlags::None;
        }
    }

    output.barcode.cornersInOriginal.clear();
    output.barcode.cornersInOriginal.reserve(
                output.barcode.cornersInRoi.size());
    for (const cv::Point2f &corner : output.barcode.cornersInRoi) {
        const cv::Point2f rotatedPoint(
                    corner.x + oriented.barcode.roi.x,
                    corner.y + oriented.barcode.roi.y);
        output.barcode.cornersInOriginal.push_back(
                    DetectionRoiGeometry::mapAffinePoint(
                        oriented.barcode.inverseRotationMatrix,
                        rotatedPoint));
    }

    if (!output.barcode.readable) {
        output.barcodeState = QStringLiteral("不可读");
        QString reason;
        if (output.barcode.status == BarcodeReadStatus::Timeout) {
            reason = QStringLiteral("二维码读取超时");
        } else if (output.barcode.status
                   == BarcodeReadStatus::InvalidRoi) {
            reason = QStringLiteral("二维码区域无效");
        } else if (output.barcode.status
                   == BarcodeReadStatus::InternalError) {
            reason = QStringLiteral(
                        "二维码解码器内部错误");
        } else {
            reason = QStringLiteral(
                        "二维码不可读或"
                        "区域内没有二维码");
        }
        finishNg(reason);
        return output;
    }

    output.barcodeState = QStringLiteral("可读");
    if (!output.dateRoiValid) {
        finishNg(QStringLiteral(
                     "日期检测区域无效"
                     "或超出图像范围"));
        output.barcodeWordResult.barcodeIsReadable = true;
        return output;
    }

    const WordDetectionPipeline wordPipeline;
    output.wordOutput = wordPipeline.detectPreparedDateRoi(
                item,
                oriented.date,
                targetText,
                templateName,
                preparedTemplates,
                templateTargetIndexes,
                thresholdPercent);
    output.barcodeWordResult = detect(
                true,
                [&output]() {
        BarcodeWordDateDetectionResult dateResult;
        dateResult.resultProduced =
                output.wordOutput.detectionResult.status
                == DetectionStatus::Completed;
        dateResult.isOk = dateResult.resultProduced
                && output.wordOutput.detectionResult.verdict
                == AlgorithmVerdict::Ok;
        return dateResult;
    });

    if (!output.barcodeWordResult.dateResultProduced) {
        finishNg(QStringLiteral(
                     "日期检测未产生有效结果"));
        return output;
    }

    result = output.wordOutput.detectionResult;
    result.modeId = detectionModeUiId(DetectionMode::BarcodeWord);
    result.elapsedMs = elapsedMs();
    output.dateState = output.barcodeWordResult.dateIsOk
            ? QStringLiteral("正确")
            : QStringLiteral("错误");
    output.reason = result.diagnostic;

    bool hasBarcodeOverlay = false;
    for (const DetectionOverlayPolygon &polygon :
         result.overlay.polygons) {
        if (polygon.role == QLatin1String("barcode")) {
            hasBarcodeOverlay = true;
            break;
        }
    }
    if (!hasBarcodeOverlay && !item.pose.barcodePoly.empty()) {
        DetectionOverlayPolygon barcodePolygon;
        barcodePolygon.role = QStringLiteral("barcode");
        barcodePolygon.points = item.pose.barcodePoly;
        result.overlay.polygons.push_back(barcodePolygon);
    }
    return output;
}
