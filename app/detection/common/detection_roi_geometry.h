#pragma once

#include "TrackingTypes.h"

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

inline cv::Point2f mapAffinePoint(
    const cv::Mat &affine,
    const cv::Point2f &point)
{
    return cv::Point2f(
        static_cast<float>(
            affine.at<double>(0, 0) * point.x
            + affine.at<double>(0, 1) * point.y
            + affine.at<double>(0, 2)),
        static_cast<float>(
            affine.at<double>(1, 0) * point.x
            + affine.at<double>(1, 1) * point.y
            + affine.at<double>(1, 2)));
}

inline std::vector<cv::Point> mapAffinePolygon(
    const std::vector<cv::Point> &polygon,
    const cv::Mat &affine)
{
    std::vector<cv::Point> mapped;
    mapped.reserve(polygon.size());
    for (const cv::Point &point : polygon) {
        const cv::Point2f result = mapAffinePoint(
                    affine,
                    cv::Point2f(
                        static_cast<float>(point.x),
                        static_cast<float>(point.y)));
        mapped.push_back(cv::Point(
                             cvRound(result.x),
                             cvRound(result.y)));
    }
    return mapped;
}

inline OrientedDateRoi prepareOrientedDateRoi(
    const cv::Mat &source,
    const DetectionPose &pose,
    int padding)
{
    OrientedDateRoi oriented;
    if (source.empty()
            || !pose.valid
            || pose.datePoly.size() < 3) {
        return oriented;
    }

    oriented.rotationMatrix = cv::getRotationMatrix2D(
                pose.anchorCenter,
                -pose.angleDeg,
                1.0);
    cv::invertAffineTransform(
                oriented.rotationMatrix,
                oriented.inverseRotationMatrix);
    cv::warpAffine(
                source,
                oriented.rotatedImage,
                oriented.rotationMatrix,
                source.size(),
                cv::INTER_LINEAR,
                cv::BORDER_REPLICATE);

    oriented.rotatedDatePoly = mapAffinePolygon(
                pose.datePoly,
                oriented.rotationMatrix);
    if (oriented.rotatedDatePoly.size() < 3) {
        return oriented;
    }

    oriented.roi = polygonRoiWithClampedPadding(
                oriented.rotatedDatePoly,
                padding,
                oriented.rotatedImage.size(),
                &oriented.rotatedDatePoly);
    if (oriented.roi.width <= 0
            || oriented.roi.height <= 0) {
        return oriented;
    }

    oriented.croppedImage = oriented.rotatedImage(
                oriented.roi).clone();
    if (oriented.croppedImage.type() != CV_8UC3) {
        cv::Mat converted;
        if (oriented.croppedImage.channels() == 1) {
            cv::cvtColor(
                        oriented.croppedImage,
                        converted,
                        cv::COLOR_GRAY2BGR);
        } else if (oriented.croppedImage.channels() == 4) {
            cv::cvtColor(
                        oriented.croppedImage,
                        converted,
                        cv::COLOR_BGRA2BGR);
        } else {
            converted = oriented.croppedImage.clone();
        }
        oriented.croppedImage = converted;
    }

    oriented.valid = !oriented.croppedImage.empty();
    return oriented;
}

} // namespace DetectionRoiGeometry
