#ifndef DETECTION_OCR_OCR_DETECTION_PIPELINE_H
#define DETECTION_OCR_OCR_DETECTION_PIPELINE_H

#include <opencv2/core.hpp>

#include <functional>
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
    typedef std::function<std::vector<std::string>(cv::Mat &image)>
            RecognizeFunction;

    OcrDetectionResult detect(
            cv::Mat &croppedImage,
            const std::string &targetText,
            const RecognizeFunction &recognize) const;
};

#endif // DETECTION_OCR_OCR_DETECTION_PIPELINE_H
