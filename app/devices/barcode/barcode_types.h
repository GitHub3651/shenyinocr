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

enum class BarcodeFormat
{
    Unknown = 0,
    DataMatrix = 1,
    QRCode = 2
};

enum class BarcodeReadStatus
{
    Success,
    NotFound,
    InvalidRoi,
    Timeout,
    DecoderUnavailable,
    InternalError
};

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
