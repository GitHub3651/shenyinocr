#pragma once

#include <opencv2/core.hpp>

#include <vector>

namespace PaddleOCR {

class Normalize
{
public:
    void Run(cv::Mat *image,
             const std::vector<float> &mean,
             const std::vector<float> &scale,
             bool isScale = true) const;
};

class Permute
{
public:
    void Run(const cv::Mat *image, float *data) const;
};

class DetResizeImg
{
public:
    void Run(const cv::Mat &image,
             cv::Mat &resizedImage,
             int maxSideLen) const;
};

class RecResizeImg
{
public:
    int Run(const cv::Mat &image,
            cv::Mat &resizedImage,
            const std::vector<int> &imageShape = {3, 48, 320}) const;
};

} // namespace PaddleOCR
