#ifndef DETECTION_OCR_OCR_DETECTION_PIPELINE_H
#define DETECTION_OCR_OCR_DETECTION_PIPELINE_H

#include "devices/ocr/ocr_engine.h"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

struct OcrDetectionResult
{
    std::string recognizedText;
    bool isOk = false;
};

class OcrDetectionPipeline
{
public:
    OcrDetectionResult detect(
            cv::Mat &croppedImage,
            const std::string &targetText,
            IOcrEngine &ocrEngine) const;
};

#endif // DETECTION_OCR_OCR_DETECTION_PIPELINE_H
