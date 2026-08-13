#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <vector>

namespace DetectionRoiGeometry {

inline cv::Rect expandAndClampRect(
    const cv::Rect &rect,
    int padding,
    const cv::Size &imageSize)
{
    if (rect.width <= 0 || rect.height <= 0
            || imageSize.width <= 0 || imageSize.height <= 0) {
        return cv::Rect();
    }

    const int safePadding = (std::max)(0, padding);
    const cv::Rect expanded(
                rect.x - safePadding,
                rect.y - safePadding,
                rect.width + safePadding * 2,
                rect.height + safePadding * 2);
    return expanded & cv::Rect(0, 0, imageSize.width, imageSize.height);
}

inline std::vector<cv::Point> clampPolygonToImage(
    const std::vector<cv::Point> &polygon,
    const cv::Size &imageSize)
{
    std::vector<cv::Point> clamped;
    if (polygon.empty()
            || imageSize.width <= 0 || imageSize.height <= 0) {
        return clamped;
    }

    const int maxX = imageSize.width - 1;
    const int maxY = imageSize.height - 1;
    clamped.reserve(polygon.size());
    for (const cv::Point &point : polygon) {
        clamped.push_back(cv::Point(
                              (std::max)(0, (std::min)(point.x, maxX)),
                              (std::max)(0, (std::min)(point.y, maxY))));
    }
    return clamped;
}

inline cv::Rect polygonRoiWithClampedPadding(
    const std::vector<cv::Point> &polygon,
    int padding,
    const cv::Size &imageSize,
    std::vector<cv::Point> *clampedPolygon = nullptr)
{
    const std::vector<cv::Point> boundedPolygon =
            clampPolygonToImage(polygon, imageSize);
    if (clampedPolygon) {
        *clampedPolygon = boundedPolygon;
    }
    if (boundedPolygon.empty()) {
        return cv::Rect();
    }

    return expandAndClampRect(
                cv::boundingRect(boundedPolygon),
                padding,
                imageSize);
}

} // namespace DetectionRoiGeometry
