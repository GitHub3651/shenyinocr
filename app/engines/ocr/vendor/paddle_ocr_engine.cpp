#include "paddle_ocr_engine.h"

#include "engines/ocr/vendor/paddle/include/config.h"
#include "engines/ocr/vendor/paddle/include/ocr_det.h"
#include "engines/ocr/vendor/paddle/include/ocr_rec.h"

#include "system_support/logging/log_categories.h"

struct PaddleOcrEngine::Impl
{
    std::unique_ptr<PaddleOCR::OCRConfig> config;
    std::unique_ptr<PaddleOCR::DBDetector> detector;
    std::unique_ptr<PaddleOCR::Classifier> classifier;
    std::unique_ptr<PaddleOCR::CRNNRecognizer> recognizer;
};

PaddleOcrEngine::PaddleOcrEngine(const QString &configPath)
    : m_impl(new Impl)
{
    m_impl->config.reset(
                new PaddleOCR::OCRConfig(configPath.toStdString()));

    const PaddleOCR::OCRConfig &config = *m_impl->config;
    m_impl->detector.reset(
                new PaddleOCR::DBDetector(
                    config.det_model_dir,
                    config.use_gpu,
                    config.gpu_id,
                    config.gpu_mem,
                    config.cpu_math_library_num_threads,
                    config.use_mkldnn,
                    config.max_side_len,
                    config.det_db_thresh,
                    config.det_db_box_thresh,
                    config.det_db_unclip_ratio,
                    config.visualize,
                    config.use_tensorrt,
                    config.use_fp16));
    if (config.use_angle_cls) {
        m_impl->classifier.reset(
                    new PaddleOCR::Classifier(
                        config.cls_model_dir,
                        config.use_gpu,
                        config.gpu_id,
                        config.gpu_mem,
                        config.cpu_math_library_num_threads,
                        config.use_mkldnn,
                        config.cls_thresh,
                        config.use_tensorrt,
                        config.use_fp16));
    }

    m_impl->recognizer.reset(
                new PaddleOCR::CRNNRecognizer(
                    config.rec_model_dir,
                    config.use_gpu,
                    config.gpu_id,
                    config.gpu_mem,
                    config.cpu_math_library_num_threads,
                    config.use_mkldnn,
                    config.char_list_file,
                    config.use_tensorrt,
                    config.use_fp16));
    qCInfo(logDevice).noquote()
            << QStringLiteral(
                "event=ocr.engine_loaded config=%1 classifier=%2 useGpu=%3")
               .arg(configPath)
               .arg(config.use_angle_cls
                    ? QStringLiteral("enabled")
                    : QStringLiteral("disabled"))
               .arg(config.use_gpu
                    ? QStringLiteral("enabled")
                    : QStringLiteral("disabled"));
}

PaddleOcrEngine::~PaddleOcrEngine() = default;

std::vector<std::string> PaddleOcrEngine::recognize(cv::Mat &image)
{
    std::vector<std::vector<std::vector<int>>> boxes;
    m_impl->detector->Run(image, boxes);

    std::vector<std::string> rawText;
    m_impl->recognizer->Run(
                boxes,
                image,
                m_impl->classifier.get(),
                rawText);
    return rawText;
}
