// 文件作用：本文件用于执行二维码与三期字符组合模式的定位、解码、字符检查和结果生成。
// 主要职责：执行二维码与三期字符组合模式的定位、解码、字符检查和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "barcode_word_detection_pipeline.h"

#include "detection/common/detection_roi_geometry.h"
#include "devices/barcode/barcode_decoder.h"

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
    output.barcodeState = QStringLiteral("\u672a\u6267\u884c");
    output.dateState = QStringLiteral("\u672a\u6267\u884c");

    DetectionResult &result = output.detectionResult;
    result.modeId = QStringLiteral("barcode_word_detection");
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
                     "\u672a\u627e\u5230\u5b9a\u4f4d\u951a\u70b9"));
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
                     "\u4e8c\u7ef4\u7801\u533a\u57df\u914d\u7f6e"
                     "\u65e0\u6548\u6216\u672a\u6620\u5c04"));
        return output;
    }
    if (item.pose.datePoly.size() < 3) {
        finishNg(QStringLiteral(
                     "\u65e5\u671f\u68c0\u6d4b\u533a\u57df\u914d\u7f6e"
                     "\u65e0\u6548\u6216\u672a\u6620\u5c04"));
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
                     "\u4e8c\u7ef4\u7801\u533a\u57df\u65e0\u6548"
                     "\u6216\u8d85\u51fa\u56fe\u50cf\u8303\u56f4"));
        return output;
    }

    if (!decoder || !decoder->ensureLoaded()) {
        output.barcode.status = BarcodeReadStatus::DecoderUnavailable;
        output.barcode.errorReason = decoder
                ? decoder->lastError()
                : QStringLiteral("Barcode decoder is null");
        output.barcodeState = QStringLiteral(
                    "\u8bfb\u7801\u5668\u4e0d\u53ef\u7528");
        finishNg(QStringLiteral("BarcodeDecoder.dll\u4e0d\u53ef\u7528"));
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
        output.barcodeState = QStringLiteral("\u4e0d\u53ef\u8bfb");
        QString reason;
        if (output.barcode.status == BarcodeReadStatus::Timeout) {
            reason = QStringLiteral("\u4e8c\u7ef4\u7801\u8bfb\u53d6\u8d85\u65f6");
        } else if (output.barcode.status
                   == BarcodeReadStatus::InvalidRoi) {
            reason = QStringLiteral("\u4e8c\u7ef4\u7801\u533a\u57df\u65e0\u6548");
        } else if (output.barcode.status
                   == BarcodeReadStatus::InternalError) {
            reason = QStringLiteral(
                        "\u4e8c\u7ef4\u7801\u89e3\u7801\u5668\u5185\u90e8\u9519\u8bef");
        } else {
            reason = QStringLiteral(
                        "\u4e8c\u7ef4\u7801\u4e0d\u53ef\u8bfb\u6216"
                        "\u533a\u57df\u5185\u6ca1\u6709\u4e8c\u7ef4\u7801");
        }
        finishNg(reason);
        return output;
    }

    output.barcodeState = QStringLiteral("\u53ef\u8bfb");
    if (!output.dateRoiValid) {
        finishNg(QStringLiteral(
                     "\u65e5\u671f\u68c0\u6d4b\u533a\u57df\u65e0\u6548"
                     "\u6216\u8d85\u51fa\u56fe\u50cf\u8303\u56f4"));
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
                     "\u65e5\u671f\u68c0\u6d4b\u672a\u4ea7\u751f\u6709\u6548\u7ed3\u679c"));
        return output;
    }

    result = output.wordOutput.detectionResult;
    result.modeId = QStringLiteral("barcode_word_detection");
    result.elapsedMs = elapsedMs();
    output.dateState = output.barcodeWordResult.dateIsOk
            ? QStringLiteral("\u6b63\u786e")
            : QStringLiteral("\u9519\u8bef");
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
