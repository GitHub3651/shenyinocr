#ifndef BARCODETYPES_H
#define BARCODETYPES_H

#include "contracts/barcode_parameter_defaults.h"

#include "BarcodeDecoderApi.h"

#include <QByteArray>
#include <QMetaType>
#include <QString>

#include <opencv2/core.hpp>

#include <vector>

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

#endif // BARCODETYPES_H
