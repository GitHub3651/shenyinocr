// 文件作用：本文件用于把二维码供应商接口转换为项目内部统一的二维码解码端口。
// 主要职责：把二维码供应商接口转换为项目内部统一的二维码解码端口。
// 模块位置：设备层；通过统一端口隔离相机、PLC、OCR和二维码供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef BARCODE_DECODER_ADAPTER_H
#define BARCODE_DECODER_ADAPTER_H

#include "devices/barcode/barcode_decoder.h"
#include "devices/barcode/vendor/barcode_decoder_api.h"

#include <QString>

#include <memory>

// 组件说明：BarcodeDecoderFunctions 数据结构集中保存该流程需要的一组相关数据。
struct BarcodeDecoderFunctions
{
    BarcodeDecoderGetVersionFunction getVersion = nullptr;
    BarcodeDecoderDecodeLuma8Function decodeLuma8 = nullptr;
};

// 组件说明：BarcodeDecoderAdapter 组件封装本文件中与其名称对应的单一职责。
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
