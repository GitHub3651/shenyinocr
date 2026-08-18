// 文件作用：本文件用于定义二维码识别结果、区域和状态等跨模块公共数据。
// 主要职责：定义二维码识别结果、区域和状态等跨模块公共数据。
// 模块位置：引擎层；通过统一端口隔离OCR和二维码供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DEVICES_BARCODE_BARCODE_TYPES_H
#define DEVICES_BARCODE_BARCODE_TYPES_H

#include "contracts/barcode_parameter_defaults.h"

#include <QByteArray>
#include <QMetaType>
#include <QString>

#include <opencv2/core.hpp>

#include <vector>

namespace BarcodeDecodeOptionFlags {
constexpr unsigned int None = 0u;
constexpr unsigned int TryHarder = 1u << 0;
constexpr unsigned int TryRotate = 1u << 1;
constexpr unsigned int TryInvert = 1u << 2;
}

// 组件说明：BarcodeFormat 枚举列出该组件允许使用的稳定状态和选项。
enum class BarcodeFormat
{
    Unknown = 0,
    DataMatrix = 1,
    QRCode = 2
};

// 组件说明：BarcodeReadStatus 枚举列出该组件允许使用的稳定状态和选项。
enum class BarcodeReadStatus
{
    Success,
    NotFound,
    InvalidRoi,
    Timeout,
    DecoderUnavailable,
    InternalError
};

// 组件说明：BarcodeDecodeOptions 组件集中描述相关配置、规则和运行参数。
struct BarcodeDecodeOptions
{
    unsigned int formatMask =
            BarcodeParameterDefaults::FormatMask;
    int roiPaddingPercent =
            BarcodeParameterDefaults::RoiPaddingPercent;
    int maxDecodeTimeMs =
            BarcodeParameterDefaults::MaxDecodeTimeMs;
    bool enableFallback =
            BarcodeParameterDefaults::EnableFallback;
};

// 组件说明：BarcodeReadResult 数据结构保存一次操作的结果、状态和错误信息。
struct BarcodeReadResult
{
    BarcodeReadStatus status = BarcodeReadStatus::NotFound;
    bool readable = false;
    BarcodeFormat format = BarcodeFormat::Unknown;
    QString text;
    QByteArray rawBytes;
    std::vector<cv::Point2f> cornersInRoi;
    std::vector<cv::Point2f> cornersInOriginal;
    double elapsedMs = 0.0;
    QString errorReason;
};

Q_DECLARE_METATYPE(BarcodeReadResult)

#endif // DEVICES_BARCODE_BARCODE_TYPES_H
