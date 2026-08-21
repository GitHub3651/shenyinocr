// 文件作用：校验并转换各检测模式的模板绘图坐标。
// 模块位置：应用层；不访问 UI 控件、磁盘、设备或检测算法。
#include "application/template_geometry_service.h"

#include <QtGlobal>

#include <cmath>

namespace {

bool validGeometry(const TemplateDisplayGeometry &geometry)
{
    return geometry.viewSize.width() > 0
            && geometry.viewSize.height() > 0
            && geometry.displayedImageSize.width() > 0
            && geometry.displayedImageSize.height() > 0
            && geometry.sourceImageSize.width() > 0
            && geometry.sourceImageSize.height() > 0;
}

QPointF mapDisplayPoint(
        const QPoint &point,
        const TemplateDisplayGeometry &geometry)
{
    const double offsetX =
            (geometry.viewSize.width()
             - geometry.displayedImageSize.width()) / 2.0;
    const double offsetY =
            (geometry.viewSize.height()
             - geometry.displayedImageSize.height()) / 2.0;
    const double scaleX =
            static_cast<double>(geometry.sourceImageSize.width())
            / geometry.displayedImageSize.width();
    const double scaleY =
            static_cast<double>(geometry.sourceImageSize.height())
            / geometry.displayedImageSize.height();
    return QPointF((point.x() - offsetX) * scaleX,
                   (point.y() - offsetY) * scaleY);
}

QRectF mapTemplateRect(
        const QRect &displayRect,
        const TemplateDisplayGeometry &geometry)
{
    const QRect normalized = displayRect.normalized();
    const QPointF topLeft = mapDisplayPoint(
                normalized.topLeft(), geometry);
    const QPointF bottomRight = mapDisplayPoint(
                normalized.bottomRight(), geometry);
    return QRectF(topLeft.x(), topLeft.y(),
                  bottomRight.x() - topLeft.x(),
                  bottomRight.y() - topLeft.y())
            .intersected(QRectF(
                0.0, 0.0,
                geometry.sourceImageSize.width(),
                geometry.sourceImageSize.height()));
}

cv::Rect roundedImageRect(const QRectF &rect)
{
    return cv::Rect(
                cvRound(rect.x()),
                cvRound(rect.y()),
                cvRound(rect.width()),
                cvRound(rect.height()));
}

void appendRelativePolygon(
        const QPolygon &displayPolygon,
        const QPointF &center,
        const TemplateDisplayGeometry &geometry,
        std::vector<cv::Point2f> *result)
{
    if (!result) {
        return;
    }
    for (const QPoint &displayPoint : displayPolygon) {
        const QPointF point = mapDisplayPoint(displayPoint, geometry);
        result->emplace_back(
                    static_cast<float>(point.x() - center.x()),
                    static_cast<float>(point.y() - center.y()));
    }
}

bool hasUnexpectedGeometry(const TemplateDrawingInput &input)
{
    if (input.mode != DetectionMode::BarcodeWord
            && !input.barcodeRect.isNull()) {
        return true;
    }
    if (input.mode != DetectionMode::Stamp
            && (!input.stampAnchorRect.isNull()
                || !input.stampPolygon.isEmpty())) {
        return true;
    }
    return false;
}

} // namespace

QRect TemplateGeometryService::mapDisplayRectToImage(
        const QRect &displayRect,
        const TemplateDisplayGeometry &geometry) const
{
    if (!validGeometry(geometry)) {
        return QRect();
    }
    const QRect normalized = displayRect.normalized();
    const QPointF topLeft = mapDisplayPoint(
                normalized.topLeft(), geometry);
    const QPointF bottomRight = mapDisplayPoint(
                QPoint(normalized.right() + 1,
                       normalized.bottom() + 1),
                geometry);
    QRect mapped(
                static_cast<int>(std::floor(topLeft.x())),
                static_cast<int>(std::floor(topLeft.y())),
                static_cast<int>(std::ceil(bottomRight.x()))
                    - static_cast<int>(std::floor(topLeft.x())),
                static_cast<int>(std::ceil(bottomRight.y()))
                    - static_cast<int>(std::floor(topLeft.y())));
    return mapped.intersected(
                QRect(QPoint(0, 0), geometry.sourceImageSize));
}

TemplateGeometryResult TemplateGeometryService::buildGeometry(
        const TemplateDrawingInput &input,
        const TemplateDisplayGeometry &geometry) const
{
    TemplateGeometryResult result;
    if (!validGeometry(geometry)) {
        result.errorMessage = QStringLiteral(
                    "模板显示尺寸或原图尺寸无效。");
        return result;
    }
    if (input.mode == DetectionMode::Tissue) {
        result.errorMessage = QStringLiteral(
                    "纸巾检测不使用模板绘图区域。");
        return result;
    }
    if (hasUnexpectedGeometry(input)) {
        result.errorMessage = QStringLiteral(
                    "当前检测模式包含不适用的模板区域，请重新框选。");
        return result;
    }

    const QRectF trackingPhysical = mapTemplateRect(
                input.trackingAnchorRect, geometry);
    const cv::Rect tracking = roundedImageRect(trackingPhysical);
    if (tracking.width <= 5 || tracking.height <= 5) {
        result.errorMessage = QStringLiteral(
                    "定位区域转换后无效，模板未保存。");
        return result;
    }
    if (input.datePolygon.size() < 3) {
        result.errorMessage = QStringLiteral(
                    "日期或文字检测区域点数不足，模板未保存。");
        return result;
    }

    result.trackingImageRect = tracking;
    result.trackingRoi = QRectF(
                tracking.x, tracking.y,
                tracking.width, tracking.height);
    const QPointF trackingCenter = result.trackingRoi.center();
    appendRelativePolygon(
                input.datePolygon,
                trackingCenter,
                geometry,
                &result.datePolygon);

    if (input.mode == DetectionMode::BarcodeWord) {
        const QRectF barcode = mapTemplateRect(
                    input.barcodeRect, geometry);
        if (barcode.width() <= 5.0 || barcode.height() <= 5.0) {
            result.errorMessage = QStringLiteral(
                        "二维码区域转换后无效，模板未保存。");
            return result;
        }
        const QPointF corners[] = {
            barcode.topLeft(),
            QPointF(barcode.right(), barcode.top()),
            barcode.bottomRight(),
            QPointF(barcode.left(), barcode.bottom())
        };
        for (const QPointF &point : corners) {
            result.barcodePolygon.emplace_back(
                        static_cast<float>(
                            point.x() - trackingCenter.x()),
                        static_cast<float>(
                            point.y() - trackingCenter.y()));
        }
    }

    if (input.mode == DetectionMode::Stamp) {
        const QRectF stampAnchor = mapTemplateRect(
                    input.stampAnchorRect, geometry);
        result.stampAnchorImageRect = roundedImageRect(stampAnchor);
        if (result.stampAnchorImageRect.width <= 5
                || result.stampAnchorImageRect.height <= 5) {
            result.errorMessage = QStringLiteral(
                        "吸管口定位锚点转换后无效，模板未保存。");
            return result;
        }
        if (input.stampPolygon.size() < 3) {
            result.errorMessage = QStringLiteral(
                        "钢印检测区域点数不足，模板未保存。");
            return result;
        }
        const QPointF stampCenter(
                    result.stampAnchorImageRect.x
                    + result.stampAnchorImageRect.width / 2.0,
                    result.stampAnchorImageRect.y
                    + result.stampAnchorImageRect.height / 2.0);
        appendRelativePolygon(
                    input.stampPolygon,
                    stampCenter,
                    geometry,
                    &result.stampPolygon);
    }

    result.valid = true;
    return result;
}
