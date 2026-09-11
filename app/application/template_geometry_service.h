#pragma once

#include "contracts/detection_mode.h"

#include <QPolygon>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>

#include <opencv2/core.hpp>

#include <vector>

struct TemplateDisplayGeometry
{
    QSize viewSize;
    QSize displayedImageSize;
    QSize sourceImageSize;
};

struct TemplateDrawingInput
{
    DetectionMode mode = DetectionMode::Tissue;
    QRect trackingAnchorRect;
    QRect barcodeRect;
    QRect stampAnchorRect;
    QPolygon datePolygon;
    QPolygon stampPolygon;
};

struct TemplateGeometryResult
{
    bool valid = false;
    QString errorMessage;
    QRectF trackingRoi;
    cv::Rect trackingImageRect;
    cv::Rect stampAnchorImageRect;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
    std::vector<cv::Point2f> stampPolygon;
};

// Pure coordinate conversion used by template editing. It has no UI, disk,
// device or template-store side effects.
class TemplateGeometryService
{
public:
    QRect mapDisplayRectToImage(
        const QRect &displayRect,
        const TemplateDisplayGeometry &geometry) const;

    TemplateGeometryResult buildGeometry(
        const TemplateDrawingInput &input,
        const TemplateDisplayGeometry &geometry) const;
};
