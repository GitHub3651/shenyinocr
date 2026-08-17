#ifndef BARCODE_DECODER_ADAPTER_H
#define BARCODE_DECODER_ADAPTER_H

#include "devices/barcode/barcode_decoder.h"
#include "devices/barcode/vendor/barcode_decoder_api.h"

#include <QString>

#include <memory>

struct BarcodeDecoderFunctions
{
    BarcodeDecoderGetVersionFunction getVersion = nullptr;
    BarcodeDecoderDecodeLuma8Function decodeLuma8 = nullptr;
};

class BarcodeDecoderAdapter final : public IBarcodeDecoder
{
public:
    BarcodeDecoderAdapter();
    explicit BarcodeDecoderAdapter(
        const BarcodeDecoderFunctions &functions);
    ~BarcodeDecoderAdapter() override;

    bool ensureLoaded() override;
    QString lastError() const override;
    BarcodeReadResult decode(
        const cv::Mat &grayRoi,
        const BarcodeDecodeOptions &options,
        int preferredStrategyId = -1,
        unsigned int preferredOptionFlags =
            BARCODE_DECODER_OPTION_NONE,
        int *successfulStrategyId = nullptr,
        unsigned int *successfulOptionFlags = nullptr) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    BarcodeReadResult decodeOnce(
        const cv::Mat &grayRoi,
        unsigned int formatMask,
        unsigned int optionFlags) const;
};

#endif // BARCODE_DECODER_ADAPTER_H
