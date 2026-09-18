#pragma once

#include <opencv2/core.hpp>

#include <vector>

namespace PaddleOCR {

class PostProcessor
{
public:
    std::vector<std::vector<std::vector<int>>> BoxesFromBitmap(
            const cv::Mat &prediction,
            const cv::Mat &bitmap,
            int destinationWidth,
            int destinationHeight,
            float boxThreshold,
            float unclipRatio) const;

private:
    static std::vector<cv::Point2f> GetMiniBox(
            const std::vector<cv::Point2f> &contour,
            float *shortSide);
    static float BoxScoreFast(
            const cv::Mat &prediction,
            const std::vector<cv::Point2f> &box);
    static std::vector<cv::Point2f> Unclip(
            const std::vector<cv::Point2f> &box,
            float unclipRatio);
};

} // namespace PaddleOCR
