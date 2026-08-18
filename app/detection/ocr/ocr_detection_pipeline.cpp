// 文件作用：本文件用于执行深度OCR模式的预处理、文字识别、目标比较和结果生成。
// 主要职责：执行深度OCR模式的预处理、文字识别、目标比较和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "ocr_detection_pipeline.h"
#include "contracts/detection_mode.h"
#include "detection/common/detection_roi_geometry.h"

#include <algorithm>
#include <chrono>
#include <cctype>

namespace {

// 函数说明：isAllowedRecognitionByte 函数检查相关状态并返回判断结果。
bool isAllowedRecognitionByte(char value)
{
    const unsigned char byte = static_cast<unsigned char>(value);
    return std::isalnum(byte)
            || (byte & 0x80) != 0
            || value == '-'
            || value == '.'
            || value == ':';
}

// 函数说明：cleanRecognitionText 函数实现名称所表示的处理步骤。
std::string cleanRecognitionText(const std::string &text)
{
    std::string cleaned = text;
    cleaned.erase(
                std::remove_if(
                    cleaned.begin(),
                    cleaned.end(),
                    [](char value) {
        return !isAllowedRecognitionByte(value);
    }),
                cleaned.end());
    return cleaned;
}

} // namespace

// 函数说明：detect 函数执行对应事件或业务处理。
OcrDetectionResult OcrDetectionPipeline::detect(
        cv::Mat &croppedImage,
        const std::string &targetText,
        IOcrEngine &ocrEngine) const
{
    OcrDetectionResult result;
    if (croppedImage.empty()) {
        return result;
    }

    const std::vector<std::string> rawText =
            ocrEngine.recognize(croppedImage);
    for (const std::string &line : rawText) {
        const std::string cleaned = cleanRecognitionText(line);
        if (cleaned.empty()) {
            continue;
        }
        if (!result.recognizedText.empty()) {
            result.recognizedText += '\n';
        }
        result.recognizedText += cleaned;
    }

    result.isOk = !result.recognizedText.empty()
            && result.recognizedText == targetText;
    return result;
}

// 函数说明：detect 函数执行对应事件或业务处理。
DetectionResult OcrDetectionPipeline::detect(
        const DetectionWorkItem &item,
        const std::string &targetText,
        IOcrEngine &ocrEngine) const
{
    const std::chrono::high_resolution_clock::time_point start =
            std::chrono::high_resolution_clock::now();
    OrientedDateRoi oriented;
    if (item.isValid() && item.hasPose) {
        oriented = DetectionRoiGeometry::prepareOrientedDateRoi(
                    item.frame->originalImage,
                    item.pose,
                    0);
    }
    if (!oriented.valid) {
        DetectionResult invalidResult;
        invalidResult.modeId = detectionModeUiId(DetectionMode::Ocr);
        invalidResult.status = DetectionStatus::Cancelled;
        invalidResult.diagnostic = QStringLiteral(
                    "OCR date ROI is invalid");
        invalidResult.elapsedMs = static_cast<double>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start)
                    .count());
        return invalidResult;
    }

    cv::Mat croppedImage = oriented.croppedImage.clone();
    const OcrDetectionResult ocrResult = detect(
                croppedImage,
                targetText,
                ocrEngine);
    const double elapsedMs = static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - start)
                .count());
    return toDetectionResult(
                ocrResult,
                item.pose,
                elapsedMs);
}

// 函数说明：toDetectionResult 函数校验、转换或恢复对应数据。
DetectionResult OcrDetectionPipeline::toDetectionResult(
        const OcrDetectionResult &ocrResult,
        const DetectionPose &pose,
        double elapsedMs)
{
    DetectionResult result;
    result.modeId = detectionModeUiId(DetectionMode::Ocr);
    result.verdict = ocrResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = QString::fromStdString(
                ocrResult.recognizedText);
    result.diagnostic = ocrResult.recognizedText.empty()
            ? QStringLiteral(
                "OCR清洗后文本为空")
            : (ocrResult.isOk
               ? QStringLiteral(
                   "OCR文本与目标完全一致")
               : QStringLiteral(
                   "OCR文本与目标不一致"));
    result.elapsedMs = elapsedMs;

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
    appendPolygon(
                QStringLiteral("tracking"),
                pose.trackingPoly,
                pose.score);
    appendPolygon(
                QStringLiteral("date"),
                pose.datePoly,
                0.0);
    return result;
}
