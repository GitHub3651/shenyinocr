#pragma once

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

struct TemplateProfileGeometry
{
    bool valid = false;
    QString errorMessage;
    QRectF trackingRoi;
    cv::Rect trackingImageRect;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
};

// Pure coordinate conversion used by template editing. It has no UI, disk,
// device or recipe-store side effects.
class TemplateGeometryService
{
public:
    QRect mapDisplayRectToImage(
        const QRect &displayRect,
        const TemplateDisplayGeometry &geometry) const;

    TemplateProfileGeometry buildProfileGeometry(
        const QRect &trackingDisplayRect,
        const QRect &barcodeDisplayRect,
        const QPolygon &dateDisplayPolygon,
        bool includeBarcode,
        const TemplateDisplayGeometry &geometry) const;
};
