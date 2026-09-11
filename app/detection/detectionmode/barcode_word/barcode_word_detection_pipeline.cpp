#include "barcode_word_detection_pipeline.h"
#include "contracts/detection_mode.h"

#include "detection/common/detection_roi_geometry.h"
#include "engines/barcode/barcode_decoder.h"

#include <algorithm>
#include <stdexcept>

BarcodeWordDetectionWorkOutput BarcodeWordDetectionPipeline::detect(
        const DetectionWorkItem &item,
        const QStringList &targetUnits,
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
    result.diagnostic = QStringLiteral(
                "Invalid barcode-word detection work item");
    if (!item.isValid() || !item.hasPose) {
        throw std::invalid_argument("Invalid barcode-word detection work item");
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
    const auto finishNg = [&output, &result](
            const QString &diagnostic,
            const QString &operatorReason) {
        result.verdict = AlgorithmVerdict::Ng;
        result.recognizedText = output.barcode.text;
        result.diagnostic = diagnostic;
        output.operatorReason = operatorReason;
    };

    if (!item.pose.valid) {
        finishNg(QStringLiteral("未找到定位锚点"),
                 QStringLiteral("未找到定位参考区域"));
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
        finishNg(QStringLiteral("二维码区域配置无效或未映射"),
                 QStringLiteral("二维码检测区域未设置或无效"));
        return output;
    }
    if (item.pose.datePoly.size() < 3) {
        finishNg(QStringLiteral("日期检测区域配置无效或未映射"),
                 QStringLiteral("日期检测区域未设置或无效"));
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
        finishNg(QStringLiteral("二维码区域无效或超出图像范围"),
                 QStringLiteral("二维码检测区域无效或超出图像范围"));
        return output;
    }
    if (!output.dateRoiValid) {
        finishNg(QStringLiteral("日期检测区域无效或超出图像范围"),
                 QStringLiteral("日期检测区域无效或超出图像范围"));
        return output;
    }

    if (!decoder || !decoder->ensureLoaded()) {
        output.barcode.status = BarcodeReadStatus::DecoderUnavailable;
        output.barcode.errorReason = decoder
                ? decoder->lastError()
                : QStringLiteral("Barcode decoder is null");
        if (output.barcode.errorReason.trimmed().isEmpty()) {
            output.barcode.errorReason = QStringLiteral(
                        "BarcodeDecoder.dll不可用");
        }
        output.barcodeState = QStringLiteral(
                    "读码器不可用");
        throw std::runtime_error(
                    output.barcode.errorReason.toStdString());
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
            const QString diagnostic = output.barcode.errorReason.isEmpty()
                    ? QStringLiteral("二维码解码器内部错误")
                    : output.barcode.errorReason;
            throw std::runtime_error(
                        diagnostic.toStdString());
        } else if (output.barcode.status
                   == BarcodeReadStatus::DecoderUnavailable) {
            const QString diagnostic = output.barcode.errorReason.isEmpty()
                    ? QStringLiteral("BarcodeDecoder.dll不可用")
                    : output.barcode.errorReason;
            throw std::runtime_error(
                        diagnostic.toStdString());
        } else {
            reason = QStringLiteral(
                        "二维码不可读或"
                        "区域内没有二维码");
        }
        finishNg(reason, reason);
        return output;
    }

    output.barcodeState = QStringLiteral("可读");
    const WordDetectionPipeline wordPipeline;
    output.wordOutput = wordPipeline.detectPreparedDateRoi(
                item,
                oriented.date,
                targetUnits,
                templateName,
                preparedTemplates,
                templateTargetIndexes,
                thresholdPercent);
    output.barcodeWordResult.barcodeIsReadable = true;
    output.barcodeWordResult.dateDetectionExecuted = true;
    output.barcodeWordResult.dateIsOk =
            output.wordOutput.detectionResult.verdict == AlgorithmVerdict::Ok;
    output.barcodeWordResult.isOk = output.barcodeWordResult.dateIsOk;

    result = output.wordOutput.detectionResult;
    result.modeId = detectionModeUiId(DetectionMode::BarcodeWord);
    result.qrContent = output.barcode.text;
    output.dateState = output.barcodeWordResult.dateIsOk
            ? QStringLiteral("正确")
            : QStringLiteral("错误");
    if (!output.barcodeWordResult.dateIsOk) {
        output.operatorReason = result.diagnostic
                == QLatin1String("日期检测区域无效或超出图像范围")
                ? QStringLiteral("日期检测区域无效或超出图像范围")
                : output.wordOutput.reason;
    }

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
