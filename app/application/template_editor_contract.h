// 文件作用：定义模板编辑页面与应用服务之间的轻量命令。
#pragma once

#include "contracts/barcode_parameter_defaults.h"

#include <opencv2/core.hpp>

#include <vector>

struct TemplateBarcodeValidationOptions
{
    unsigned int formatMask = BarcodeParameterDefaults::FormatMask;
    int roiPaddingPercent = BarcodeParameterDefaults::RoiPaddingPercent;
    int maxDecodeTimeMs = BarcodeParameterDefaults::MaxDecodeTimeMs;
    bool enableFallback = BarcodeParameterDefaults::EnableFallback;
};

struct InitialTemplateAssets
{
    cv::Mat rawImage;
    cv::Rect trackingImageRect;
    std::vector<cv::Point2f> stampPolygon;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
    cv::Mat stampRing;
};
