// 文件作用：本文件用于定义二维码解码端口，使检测算法不直接依赖具体解码库。
// 主要职责：定义二维码解码端口，使检测算法不直接依赖具体解码库。
// 模块位置：引擎层；通过统一端口隔离OCR和二维码供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef BARCODE_DECODER_H
#define BARCODE_DECODER_H

#include "engines/barcode/barcode_types.h"

#include <QString>

// 组件说明：IBarcodeDecoder 组件提供对应识别或解码能力的统一实现。
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
            BarcodeDecodeOptionFlags::None,
        int *successfulStrategyId = nullptr,
        unsigned int *successfulOptionFlags = nullptr) = 0;
};

#endif // BARCODE_DECODER_H
