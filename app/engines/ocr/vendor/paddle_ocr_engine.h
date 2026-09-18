#ifndef DEVICES_OCR_PADDLE_OCR_ENGINE_H
#define DEVICES_OCR_PADDLE_OCR_ENGINE_H

#include "engines/ocr/ocr_engine.h"

#include <QString>

#include <memory>

class PaddleOcrEngine final : public IOcrEngine
{
public:
    explicit PaddleOcrEngine(const QString &configPath);
    ~PaddleOcrEngine() override;

    std::vector<OcrRecognitionItem> recognize(cv::Mat &image) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // DEVICES_OCR_PADDLE_OCR_ENGINE_H
