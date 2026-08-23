// 文件作用：本文件用于提供检测区域裁剪、边界限制和坐标还原的通用几何函数。
// 主要职责：提供检测区域裁剪、边界限制和坐标还原的通用几何函数。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/common/detection_pose.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <tuple>
#include <vector>

namespace DetectionRoiGeometry {

// 组件说明：BarcodeWordOrientedRois 数据结构集中保存该流程需要的一组相关数据。
struct BarcodeWordOrientedRois
{
    OrientedBarcodeRoi barcode;
    OrientedDateRoi date;
};

// 函数说明：expandAndClampRect 函数实现名称所表示的处理步骤。
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

// 函数说明：clampPolygonToImage 函数实现名称所表示的处理步骤。
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

// 函数说明：polygonRoiWithClampedPadding 函数实现名称所表示的处理步骤。
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

// 函数说明：mapAffinePoint 函数校验、转换或恢复对应数据。
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

// 函数说明：mapAffinePolygon 函数校验、转换或恢复对应数据。
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

// 函数说明：prepareOrientedDateRoi 函数创建、准备或启动对应流程。
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

// 函数说明：prepareBarcodeWordOrientedRois 函数创建、准备或启动对应流程。
inline BarcodeWordOrientedRois prepareBarcodeWordOrientedRois(
    const cv::Mat &source,
    const DetectionPose &pose,
    int barcodePaddingPercent,
    int datePadding)
{
    BarcodeWordOrientedRois prepared;
    if (source.empty() || !pose.valid
            || pose.barcodePoly.size() != 4) {
        return prepared;
    }

    const cv::Mat rotationMatrix = cv::getRotationMatrix2D(
                pose.anchorCenter,
                -pose.angleDeg,
                1.0);
    cv::Mat inverseRotationMatrix;
    cv::invertAffineTransform(
                rotationMatrix,
                inverseRotationMatrix);

    std::vector<cv::Point> rotatedBarcodePoly =
            mapAffinePolygon(pose.barcodePoly, rotationMatrix);
    if (rotatedBarcodePoly.size() != 4) {
        return prepared;
    }

    rotatedBarcodePoly = clampPolygonToImage(
                rotatedBarcodePoly,
                source.size());
    const cv::Rect barcodeBounds = cv::boundingRect(
                rotatedBarcodePoly);
    if (barcodeBounds.width <= 0 || barcodeBounds.height <= 0) {
        return prepared;
    }

    const int shorterBarcodeSide = (std::min)(
                barcodeBounds.width,
                barcodeBounds.height);
    const int barcodePaddingPixels = cvRound(
                static_cast<double>(shorterBarcodeSide)
                * static_cast<double>((std::max)(
                    0,
                    barcodePaddingPercent))
                / 100.0);
    const cv::Rect barcodeRoi = expandAndClampRect(
                barcodeBounds,
                barcodePaddingPixels,
                source.size());
    if (barcodeRoi.width <= 0 || barcodeRoi.height <= 0) {
        return prepared;
    }

    std::vector<cv::Point> rotatedDatePoly;
    cv::Rect dateRoi;
    bool hasValidDateRoi = false;
    if (pose.datePoly.size() >= 3) {
        rotatedDatePoly = mapAffinePolygon(
                    pose.datePoly,
                    rotationMatrix);
        if (rotatedDatePoly.size() >= 3) {
            dateRoi = polygonRoiWithClampedPadding(
                        rotatedDatePoly,
                        (std::max)(0, datePadding),
                        source.size(),
                        &rotatedDatePoly);
            hasValidDateRoi = dateRoi.width > 0
                    && dateRoi.height > 0;
        }
    }

    const cv::Rect combinedRoi = hasValidDateRoi
            ? (barcodeRoi | dateRoi)
            : barcodeRoi;
    if (combinedRoi.width <= 0 || combinedRoi.height <= 0) {
        return prepared;
    }

    cv::Mat localRotationMatrix = rotationMatrix.clone();
    localRotationMatrix.at<double>(0, 2) -= combinedRoi.x;
    localRotationMatrix.at<double>(1, 2) -= combinedRoi.y;

    cv::Mat rotatedRegion;
    cv::warpAffine(
                source,
                rotatedRegion,
                localRotationMatrix,
                combinedRoi.size(),
                cv::INTER_LINEAR,
                cv::BORDER_REPLICATE);
    if (rotatedRegion.empty()) {
        return prepared;
    }

    const cv::Rect localBarcodeRoi(
                barcodeRoi.x - combinedRoi.x,
                barcodeRoi.y - combinedRoi.y,
                barcodeRoi.width,
                barcodeRoi.height);
    const cv::Mat barcodeCrop = rotatedRegion(localBarcodeRoi);
    if (barcodeCrop.channels() == 1) {
        if (barcodeCrop.depth() == CV_8U) {
            prepared.barcode.grayRoi = barcodeCrop.clone();
        } else {
            barcodeCrop.convertTo(
                        prepared.barcode.grayRoi,
                        CV_8U);
        }
    } else if (barcodeCrop.channels() == 3) {
        cv::cvtColor(
                    barcodeCrop,
                    prepared.barcode.grayRoi,
                    cv::COLOR_BGR2GRAY);
    } else if (barcodeCrop.channels() == 4) {
        cv::cvtColor(
                    barcodeCrop,
                    prepared.barcode.grayRoi,
                    cv::COLOR_BGRA2GRAY);
    }

    if (!prepared.barcode.grayRoi.empty()
            && !prepared.barcode.grayRoi.isContinuous()) {
        prepared.barcode.grayRoi =
                prepared.barcode.grayRoi.clone();
    }

    prepared.barcode.rotatedImage = rotatedRegion;
    prepared.barcode.rotatedBarcodePoly = rotatedBarcodePoly;
    prepared.barcode.roi = barcodeRoi;
    prepared.barcode.rotationMatrix = rotationMatrix;
    prepared.barcode.inverseRotationMatrix = inverseRotationMatrix;
    prepared.barcode.valid = !prepared.barcode.grayRoi.empty()
            && prepared.barcode.grayRoi.type() == CV_8UC1
            && prepared.barcode.grayRoi.isContinuous();

    if (!hasValidDateRoi) {
        return prepared;
    }

    const cv::Rect localDateRoi(
                dateRoi.x - combinedRoi.x,
                dateRoi.y - combinedRoi.y,
                dateRoi.width,
                dateRoi.height);
    prepared.date.croppedImage =
            rotatedRegion(localDateRoi).clone();
    if (prepared.date.croppedImage.type() != CV_8UC3) {
        cv::Mat converted;
        if (prepared.date.croppedImage.channels() == 1) {
            cv::cvtColor(
                        prepared.date.croppedImage,
                        converted,
                        cv::COLOR_GRAY2BGR);
        } else if (prepared.date.croppedImage.channels() == 4) {
            cv::cvtColor(
                        prepared.date.croppedImage,
                        converted,
                        cv::COLOR_BGRA2BGR);
        } else {
            converted = prepared.date.croppedImage.clone();
        }
        prepared.date.croppedImage = converted;
    }

    prepared.date.rotatedImage = rotatedRegion;
    prepared.date.rotatedDatePoly = rotatedDatePoly;
    prepared.date.roi = dateRoi;
    prepared.date.rotationMatrix = rotationMatrix;
    prepared.date.inverseRotationMatrix = inverseRotationMatrix;
    prepared.date.valid = !prepared.date.croppedImage.empty();
    return prepared;
}

// 函数说明：mapCharacterMatchesToOverlay 函数校验、转换或恢复对应数据。
inline std::vector<DetectionOverlayPolygon> mapCharacterMatchesToOverlay(
    const std::vector<std::tuple<cv::Rect, double, size_t> > &matches,
    const OrientedDateRoi &oriented,
    const cv::Size &originalSize)
{
    std::vector<DetectionOverlayPolygon> polygons;
    if (!oriented.valid || originalSize.width <= 0
            || originalSize.height <= 0) {
        return polygons;
    }

    polygons.reserve(matches.size());
    for (const std::tuple<cv::Rect, double, size_t> &match : matches) {
        cv::Rect rect = std::get<0>(match);
        rect.x += oriented.roi.x;
        rect.y += oriented.roi.y;
        const std::vector<cv::Point> rectPolygon = {
            cv::Point(rect.x, rect.y),
            cv::Point(rect.x + rect.width, rect.y),
            cv::Point(rect.x + rect.width, rect.y + rect.height),
            cv::Point(rect.x, rect.y + rect.height)
        };
        const std::vector<cv::Point> mappedPolygon =
                mapAffinePolygon(rectPolygon,
                                 oriented.inverseRotationMatrix);
        const cv::Rect mappedBounds = cv::boundingRect(mappedPolygon)
                & cv::Rect(0, 0, originalSize.width, originalSize.height);
        if (mappedBounds.width <= 0 || mappedBounds.height <= 0) {
            continue;
        }

        DetectionOverlayPolygon polygon;
        polygon.role = QStringLiteral("character");
        polygon.points = mappedPolygon;
        polygon.score = std::get<1>(match);
        polygons.push_back(polygon);
    }
    return polygons;
}

} // namespace DetectionRoiGeometry
