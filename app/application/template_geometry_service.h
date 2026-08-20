// 文件作用：本文件用于校验并转换模板跟踪框、二维码框和日期多边形坐标。
// 主要职责：校验并转换模板跟踪框、二维码框和日期多边形坐标。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、模板和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QPolygon>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>

#include <opencv2/core.hpp>

#include <vector>

// 组件说明：TemplateDisplayGeometry 数据结构集中保存该流程需要的一组相关数据。
struct TemplateDisplayGeometry
{
    QSize viewSize;
    QSize displayedImageSize;
    QSize sourceImageSize;
};

// 组件说明：TemplateGeometryResult 数据结构集中保存该流程需要的一组相关数据。
struct TemplateGeometryResult
{
    bool valid = false;
    QString errorMessage;
    QRectF trackingRoi;
    cv::Rect trackingImageRect;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
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
        const QRect &trackingDisplayRect,
        const QRect &barcodeDisplayRect,
        const QPolygon &dateDisplayPolygon,
        bool includeBarcode,
        const TemplateDisplayGeometry &geometry) const;
};
