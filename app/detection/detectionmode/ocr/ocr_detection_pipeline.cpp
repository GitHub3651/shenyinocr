#include "ocr_detection_pipeline.h"
#include "contracts/detection_mode.h"
#include "detection/common/detection_roi_geometry.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

bool isAllowedRecognitionByte(char value)
{
    const unsigned char byte = static_cast<unsigned char>(value);
    return std::isalnum(byte)
            || (byte & 0x80) != 0
            || value == '-'
            || value == '.'
            || value == ':';
}

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

DetectionResult OcrDetectionPipeline::detect(
        const DetectionWorkItem &item,
        const std::string &targetText,
        IOcrEngine &ocrEngine) const
{
    OrientedDateRoi oriented;
    if (!item.isValid() || !item.hasPose) {
        throw std::invalid_argument("Invalid OCR detection work item");
    }
    if (!item.pose.valid) {
        DetectionResult result;
        result.modeId = detectionModeUiId(DetectionMode::Ocr);
        result.verdict = AlgorithmVerdict::Ng;
        result.diagnostic = QStringLiteral("未找到OCR定位区域");
        return result;
    }
    oriented = DetectionRoiGeometry::prepareOrientedDateRoi(
                item.frame->originalImage,
                item.pose,
                0);
    if (!oriented.valid) {
        DetectionResult invalidResult;
        invalidResult.modeId = detectionModeUiId(DetectionMode::Ocr);
        invalidResult.verdict = AlgorithmVerdict::Ng;
        invalidResult.diagnostic = QStringLiteral(
                    "OCR date ROI is invalid");
        return invalidResult;
    }

    cv::Mat croppedImage = oriented.croppedImage.clone();
    const OcrDetectionResult ocrResult = detect(
                croppedImage,
                targetText,
                ocrEngine);
    return toDetectionResult(
                ocrResult,
                item.pose);
}

DetectionResult OcrDetectionPipeline::toDetectionResult(
        const OcrDetectionResult &ocrResult,
        const DetectionPose &pose)
{
    DetectionResult result;
    result.modeId = detectionModeUiId(DetectionMode::Ocr);
    result.verdict = ocrResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
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
