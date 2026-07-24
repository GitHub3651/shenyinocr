#ifndef BARCODETYPES_H
#define BARCODETYPES_H

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
    unsigned int formatMask = BARCODE_DECODER_FORMAT_DATA_MATRIX;
    int roiPaddingPercent = 8;
    int maxDecodeTimeMs = 60;
    bool enableFallback = true;
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
    int elapsedMs = 0;
    QString errorReason;
};

Q_DECLARE_METATYPE(BarcodeReadResult)

#endif // BARCODETYPES_H
