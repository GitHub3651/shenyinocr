#ifndef DEVICES_OCR_OCR_ENGINE_H
#define DEVICES_OCR_OCR_ENGINE_H

#include <opencv2/core.hpp>

#include <string>
#include <vector>

class IOcrEngine
{
public:
    virtual ~IOcrEngine() = default;

    virtual std::vector<std::string> recognize(cv::Mat &image) = 0;
};

#endif // DEVICES_OCR_OCR_ENGINE_H
