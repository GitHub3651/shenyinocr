// 文件作用：本文件用于校验并转换模板跟踪框、二维码框和日期多边形坐标。
// 主要职责：校验并转换模板跟踪框、二维码框和日期多边形坐标。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、模板和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "application/template_geometry_service.h"

#include <QtGlobal>

#include <cmath>

namespace {

// 函数说明：validGeometry 函数实现名称所表示的处理步骤。
bool validGeometry(const TemplateDisplayGeometry &geometry)
{
    return geometry.viewSize.width() > 0
            && geometry.viewSize.height() > 0
            && geometry.displayedImageSize.width() > 0
            && geometry.displayedImageSize.height() > 0
            && geometry.sourceImageSize.width() > 0
            && geometry.sourceImageSize.height() > 0;
}

// 函数说明：mapDisplayPoint 函数校验、转换或恢复对应数据。
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

} // namespace

// 函数说明：mapDisplayRectToImage 函数校验、转换或恢复对应数据。
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

// 函数说明：buildGeometry 函数创建、准备或启动对应流程。
TemplateGeometryResult TemplateGeometryService::buildGeometry(
        const QRect &trackingDisplayRect,
        const QRect &barcodeDisplayRect,
        const QPolygon &dateDisplayPolygon,
        bool includeBarcode,
        const TemplateDisplayGeometry &geometry) const
{
    TemplateGeometryResult result;
    if (!validGeometry(geometry)) {
        result.errorMessage = QStringLiteral(
                    "模板显示尺寸或原图尺寸无效。");
        return result;
    }

    const QRectF trackingPhysical = mapTemplateRect(
                trackingDisplayRect, geometry);
    const cv::Rect tracking(
                cvRound(trackingPhysical.x()),
                cvRound(trackingPhysical.y()),
                cvRound(trackingPhysical.width()),
                cvRound(trackingPhysical.height()));
    if (tracking.width <= 5 || tracking.height <= 5) {
        result.errorMessage = QStringLiteral(
                    "定位区域转换后无效，模板未保存。");
        return result;
    }
    if (dateDisplayPolygon.size() < 3) {
        result.errorMessage = QStringLiteral(
                    "喷码检测区域点数不足，模板未保存。");
        return result;
    }

    result.trackingImageRect = tracking;
    result.trackingRoi = QRectF(
                tracking.x, tracking.y,
                tracking.width, tracking.height);
    const QPointF center = result.trackingRoi.center();
    for (const QPoint &displayPoint : dateDisplayPolygon) {
        const QPointF point = mapDisplayPoint(displayPoint, geometry);
        result.datePolygon.emplace_back(
                    static_cast<float>(point.x() - center.x()),
                    static_cast<float>(point.y() - center.y()));
    }

    if (includeBarcode) {
        const QRectF barcode = mapTemplateRect(
                    barcodeDisplayRect, geometry);
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
                        static_cast<float>(point.x() - center.x()),
                        static_cast<float>(point.y() - center.y()));
        }
    }

    result.valid = true;
    return result;
}
