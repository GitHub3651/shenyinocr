#ifndef BARCODE_DECODER_H
#define BARCODE_DECODER_H

#include "BarcodeTypes.h"

#include <QString>

class IBarcodeDecoder
{
public:
    virtual ~IBarcodeDecoder() = default;

    virtual bool ensureLoaded() = 0;
    virtual QString lastError() const = 0;
    virtual BarcodeReadResult decode(
        const cv::Mat &grayRoi,
        const BarcodeDecodeOptions &options,
        int preferredStrategyId = -1,
        unsigned int preferredOptionFlags =
            BARCODE_DECODER_OPTION_NONE,
        int *successfulStrategyId = nullptr,
        unsigned int *successfulOptionFlags = nullptr) = 0;
};

#endif // BARCODE_DECODER_H
