#ifndef DEVICES_OCR_OCR_ENGINE_H
#define DEVICES_OCR_OCR_ENGINE_H

#include <opencv2/core.hpp>

#include <QString>

#include <memory>
#include <string>
#include <vector>

struct OcrRecognitionItem
{
    std::string text;
    std::vector<cv::Point> box;
};

class IOcrEngine
{
protected:
    explicit IOcrEngine(const QString &configPath);
    ~IOcrEngine();

    std::vector<OcrRecognitionItem> recognize(cv::Mat &image);
    std::string recognizeCharacter(cv::Mat &image);
    std::vector<OcrRecognitionItem> segmentCharacters(cv::Mat &image);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // DEVICES_OCR_OCR_ENGINE_H
