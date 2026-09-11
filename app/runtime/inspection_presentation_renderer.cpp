#include "runtime/inspection_presentation_renderer.h"

#include <algorithm>
#include <string>

namespace {
cv::Point polygonTopCenter(const std::vector<cv::Point> &polygon)
{
    if (polygon.empty()) {
        return cv::Point();
    }
    if (polygon.size() == 1) {
        return polygon.front();
    }

    std::vector<cv::Point> sorted = polygon;
    std::sort(
        sorted.begin(),
        sorted.end(),
        [](const cv::Point &left, const cv::Point &right) {
        if (left.y != right.y) {
            return left.y < right.y;
        }
        return left.x < right.x;
    });
    return cv::Point(
        (sorted[0].x + sorted[1].x) / 2,
        (sorted[0].y + sorted[1].y) / 2);
}

void drawPolygon(
    cv::Mat &image,
    const std::vector<cv::Point> &polygon,
    const cv::Scalar &color,
    int thickness)
{
    if (polygon.empty()) {
        return;
    }
    const std::vector<std::vector<cv::Point> > polygons(1, polygon);
    cv::polylines(image, polygons, true, color, thickness);
}

void drawCharacter(
    cv::Mat &image,
    const DetectionOverlayPolygon &polygon,
    double fontScale,
    int boxThickness,
    int textThickness)
{
    if (polygon.points.size() < 4) {
        return;
    }

    drawPolygon(
        image,
        polygon.points,
        cv::Scalar(0, 255, 0),
        boxThickness);
    if (polygon.score < 0) {
        return;
    }

    const std::string scoreText = std::to_string(
        static_cast<int>(polygon.score * 100));
    int baseline = 0;
    const cv::Size textSize = cv::getTextSize(
        scoreText,
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        textThickness,
        &baseline);
    const cv::Point textAnchor = polygonTopCenter(polygon.points);
    const int textX = std::max(
        0,
        std::min(
            textAnchor.x - textSize.width / 2,
            image.cols - textSize.width));
    const int textY = std::max(
        textSize.height,
        std::min(textAnchor.y - 5, image.rows));

    cv::putText(
        image,
        scoreText,
        cv::Point(textX, textY),
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        cv::Scalar(0, 0, 0),
        textThickness + 2);
    cv::putText(
        image,
        scoreText,
        cv::Point(textX, textY),
        cv::FONT_HERSHEY_SIMPLEX,
        fontScale,
        cv::Scalar(0, 255, 255),
        textThickness);
}

void drawTissueEllipse(
    cv::Mat &image,
    const DetectionOverlayEllipse &ellipse)
{
    if (image.empty()) {
        return;
    }

    const double dynamicScale = std::max(1.0, image.rows / 800.0);
    const int thickness = std::max(
        2,
        static_cast<int>(2 * dynamicScale));
    const int radius = std::max(
        1,
        cvRound(std::max(ellipse.axes.width, ellipse.axes.height)));

    cv::circle(
        image,
        cv::Point(cvRound(ellipse.center.x), cvRound(ellipse.center.y)),
        radius,
        ellipse.role == QLatin1String("tissue_inner")
                ? cv::Scalar(255, 0, 0)
                : cv::Scalar(0, 255, 255),
        thickness);
}
}

QImage InspectionPresentationRenderer::renderRawFrame(
    const cv::Mat &image)
{
    return renderDetectionFrame(image, DetectionOverlay());
}

QImage InspectionPresentationRenderer::renderDetectionFrame(
    const cv::Mat &image,
    const DetectionOverlay &overlay)
{
    if (image.empty()) {
        return QImage();
    }

    cv::Mat displayImage;
    if (image.channels() == 3) {
        displayImage = image.clone();
    } else if (image.channels() == 1) {
        cv::cvtColor(image, displayImage, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, displayImage, cv::COLOR_BGRA2BGR);
    } else {
        return QImage();
    }

    const double dynamicScale = std::max(
        1.0,
        displayImage.rows / 800.0);
    const double fontScale = 0.4 * dynamicScale;
    const int boxThickness = std::max(
        2,
        static_cast<int>(2 * dynamicScale));
    const int textThickness = std::max(
        1,
        static_cast<int>(1.5 * dynamicScale));

    for (const DetectionOverlayPolygon &polygon :
         overlay.polygons) {
        if (polygon.role == QLatin1String("character")) {
            drawCharacter(
                displayImage,
                polygon,
                fontScale,
                boxThickness,
                textThickness);
        }
    }

    for (const DetectionOverlayPolygon &polygon :
         overlay.polygons) {
        if (polygon.role == QLatin1String("tracking")) {
            drawPolygon(displayImage, polygon.points,
                        cv::Scalar(255, 0, 0), boxThickness);
        } else if (polygon.role == QLatin1String("barcode")) {
            drawPolygon(displayImage, polygon.points,
                        cv::Scalar(0, 255, 255), boxThickness);
        } else if (polygon.role == QLatin1String("date")) {
            drawPolygon(displayImage, polygon.points,
                        cv::Scalar(0, 255, 0), boxThickness);
        } else if (polygon.role == QLatin1String("stamp")) {
            drawPolygon(
                displayImage,
                polygon.points,
                polygon.alarm
                ? cv::Scalar(0, 0, 255)
                : cv::Scalar(0, 255, 255),
                boxThickness);
        }
    }

    for (const DetectionOverlayEllipse &ellipse : overlay.ellipses) {
        drawTissueEllipse(displayImage, ellipse);
    }

    const QImage rendered(
        displayImage.data,
        displayImage.cols,
        displayImage.rows,
        static_cast<int>(displayImage.step),
        QImage::Format_RGB888);
    return rendered.rgbSwapped();
}
