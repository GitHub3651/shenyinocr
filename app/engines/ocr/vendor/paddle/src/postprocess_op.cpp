#include "engines/ocr/vendor/paddle/include/postprocess_op.h"

#include "engines/ocr/vendor/paddle/include/clipper.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace PaddleOCR {

std::vector<std::vector<std::vector<int>>>
PostProcessor::BoxesFromBitmap(const cv::Mat &prediction,
                               const cv::Mat &bitmap,
                               int destinationWidth,
                               int destinationHeight,
                               float boxThreshold,
                               float unclipRatio) const
{
    const int minimumSize = 3;
    const int maximumCandidates = 3000;
    const float widthScale = static_cast<float>(destinationWidth)
            / static_cast<float>(bitmap.cols);
    const float heightScale = static_cast<float>(destinationHeight)
            / static_cast<float>(bitmap.rows);

    cv::Mat bitmapUint8;
    bitmap.convertTo(bitmapUint8, CV_8UC1);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(bitmapUint8,
                     contours,
                     cv::RETR_LIST,
                     cv::CHAIN_APPROX_SIMPLE);

    std::vector<std::vector<std::vector<int>>> boxes;
    const int count = std::min(
                static_cast<int>(contours.size()),
                maximumCandidates);
    for (int index = 0; index < count; ++index) {
        std::vector<cv::Point2f> contour;
        contour.reserve(contours[index].size());
        for (const cv::Point &point : contours[index]) {
            contour.push_back(cv::Point2f(
                                  static_cast<float>(point.x),
                                  static_cast<float>(point.y)));
        }

        float shortSide = 0.0f;
        const std::vector<cv::Point2f> candidate =
                GetMiniBox(contour, &shortSide);
        if (shortSide < minimumSize) {
            continue;
        }
        if (BoxScoreFast(prediction, candidate) < boxThreshold) {
            continue;
        }

        const std::vector<cv::Point2f> expanded =
                Unclip(candidate, unclipRatio);
        if (expanded.empty()) {
            continue;
        }
        const std::vector<cv::Point2f> minimumBox =
                GetMiniBox(expanded, &shortSide);
        if (shortSide < minimumSize + 2) {
            continue;
        }

        std::vector<std::vector<int>> box;
        box.reserve(4);
        for (const cv::Point2f &point : minimumBox) {
            const int x = std::max(
                        0,
                        std::min(
                            static_cast<int>(
                                std::round(point.x * widthScale)),
                            destinationWidth - 1));
            const int y = std::max(
                        0,
                        std::min(
                            static_cast<int>(
                                std::round(point.y * heightScale)),
                            destinationHeight - 1));
            box.push_back({x, y});
        }

        boxes.push_back(box);
    }
    return boxes;
}

std::vector<cv::Point2f> PostProcessor::GetMiniBox(
        const std::vector<cv::Point2f> &contour,
        float *shortSide)
{
    const cv::RotatedRect rectangle = cv::minAreaRect(contour);
    std::vector<cv::Point2f> points(4);
    rectangle.points(points.data());
    std::sort(
                points.begin(),
                points.end(),
                [](const cv::Point2f &left, const cv::Point2f &right) {
        return left.x < right.x;
    });

    const int topLeft = points[1].y > points[0].y ? 0 : 1;
    const int bottomLeft = topLeft == 0 ? 1 : 0;
    const int topRight = points[3].y > points[2].y ? 2 : 3;
    const int bottomRight = topRight == 2 ? 3 : 2;
    *shortSide = std::min(rectangle.size.width, rectangle.size.height);
    return {points[topLeft],
            points[topRight],
            points[bottomRight],
            points[bottomLeft]};
}

float PostProcessor::BoxScoreFast(
        const cv::Mat &prediction,
        const std::vector<cv::Point2f> &box)
{
    float minimumX = box[0].x;
    float maximumX = box[0].x;
    float minimumY = box[0].y;
    float maximumY = box[0].y;
    for (const cv::Point2f &point : box) {
        minimumX = std::min(minimumX, point.x);
        maximumX = std::max(maximumX, point.x);
        minimumY = std::min(minimumY, point.y);
        maximumY = std::max(maximumY, point.y);
    }

    const int left = std::max(
                0,
                std::min(static_cast<int>(std::floor(minimumX)),
                         prediction.cols - 1));
    const int right = std::max(
                0,
                std::min(static_cast<int>(std::ceil(maximumX)),
                         prediction.cols - 1));
    const int top = std::max(
                0,
                std::min(static_cast<int>(std::floor(minimumY)),
                         prediction.rows - 1));
    const int bottom = std::max(
                0,
                std::min(static_cast<int>(std::ceil(maximumY)),
                         prediction.rows - 1));

    cv::Mat mask = cv::Mat::zeros(
                bottom - top + 1,
                right - left + 1,
                CV_8UC1);
    std::vector<cv::Point> localBox;
    localBox.reserve(box.size());
    for (const cv::Point2f &point : box) {
        localBox.push_back(cv::Point(
                               static_cast<int>(point.x - left),
                               static_cast<int>(point.y - top)));
    }
    cv::fillPoly(mask,
                 std::vector<std::vector<cv::Point>>{localBox},
                 cv::Scalar(1));
    const cv::Mat region = prediction(
                cv::Rect(left,
                         top,
                         right - left + 1,
                         bottom - top + 1));
    return static_cast<float>(cv::mean(region, mask)[0]);
}

std::vector<cv::Point2f> PostProcessor::Unclip(
        const std::vector<cv::Point2f> &box,
        float unclipRatio)
{
    const double perimeter = cv::arcLength(box, true);
    if (perimeter <= 0.0) {
        return {};
    }
    const double distance = std::fabs(cv::contourArea(box))
            * unclipRatio / perimeter;

    ClipperLib::Path path;
    for (const cv::Point2f &point : box) {
        path << ClipperLib::IntPoint(
                    static_cast<ClipperLib::cInt>(point.x),
                    static_cast<ClipperLib::cInt>(point.y));
    }
    ClipperLib::ClipperOffset offset;
    offset.AddPath(path,
                   ClipperLib::jtRound,
                   ClipperLib::etClosedPolygon);
    ClipperLib::Paths solution;
    offset.Execute(solution, distance);
    if (solution.empty()) {
        return {};
    }

    std::vector<cv::Point2f> expanded;
    expanded.reserve(solution[0].size());
    for (const ClipperLib::IntPoint &point : solution[0]) {
        expanded.push_back(cv::Point2f(
                               static_cast<float>(point.X),
                               static_cast<float>(point.Y)));
    }
    return expanded;
}

} // namespace PaddleOCR
