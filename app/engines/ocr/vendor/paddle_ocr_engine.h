// 文件作用：本文件用于把PaddleOCR实现封装为项目统一OCR引擎端口。
// 主要职责：把PaddleOCR实现封装为项目统一OCR引擎端口。
// 模块位置：引擎层；通过统一端口隔离OCR和二维码供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DEVICES_OCR_PADDLE_OCR_ENGINE_H
#define DEVICES_OCR_PADDLE_OCR_ENGINE_H

#include "engines/ocr/ocr_engine.h"

#include <QString>

#include <memory>

// 组件说明：PaddleOcrEngine 组件提供对应识别或解码能力的统一实现。
class PaddleOcrEngine final : public IOcrEngine
{
public:
    explicit PaddleOcrEngine(const QString &configPath);
    ~PaddleOcrEngine() override;

    std::vector<std::string> recognize(cv::Mat &image) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // DEVICES_OCR_PADDLE_OCR_ENGINE_H
