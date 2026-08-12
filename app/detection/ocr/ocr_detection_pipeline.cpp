#include "ocr_detection_pipeline.h"

#include <algorithm>
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
        const RecognizeFunction &recognize) const
{
    OcrDetectionResult result;
    if (croppedImage.empty() || !recognize) {
        return result;
    }

    const std::vector<std::string> rawText = recognize(croppedImage);
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
