#include "ocr_detection_pipeline.h"
#include "detection/common/detection_roi_geometry.h"

#include <algorithm>
#include <chrono>
#include <cctype>

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
        invalidResult.modeId = QStringLiteral("ocr_detection");
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

DetectionResult OcrDetectionPipeline::toDetectionResult(
        const OcrDetectionResult &ocrResult,
        const DetectionPose &pose,
        double elapsedMs)
{
    DetectionResult result;
    result.modeId = QStringLiteral("ocr_detection");
    result.verdict = ocrResult.isOk
            ? AlgorithmVerdict::Ok
            : AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = QString::fromStdString(
                ocrResult.recognizedText);
    result.diagnostic = ocrResult.recognizedText.empty()
            ? QStringLiteral(
                "\u004f\u0043\u0052\u6e05\u6d17\u540e\u6587\u672c\u4e3a\u7a7a")
            : (ocrResult.isOk
               ? QStringLiteral(
                   "\u004f\u0043\u0052\u6587\u672c\u4e0e\u76ee\u6807\u5b8c\u5168\u4e00\u81f4")
               : QStringLiteral(
                   "\u004f\u0043\u0052\u6587\u672c\u4e0e\u76ee\u6807\u4e0d\u4e00\u81f4"));
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
