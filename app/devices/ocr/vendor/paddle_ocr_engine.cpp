// 文件作用：本文件用于把PaddleOCR实现封装为项目统一OCR引擎端口。
// 主要职责：把PaddleOCR实现封装为项目统一OCR引擎端口。
// 模块位置：设备层；通过统一端口隔离相机、PLC、OCR和二维码供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "paddle_ocr_engine.h"

#include "devices/ocr/vendor/paddle/include/config.h"
#include "devices/ocr/vendor/paddle/include/ocr_det.h"
#include "devices/ocr/vendor/paddle/include/ocr_rec.h"

#include <QDebug>

// 组件说明：PaddleOcrEngine 组件提供对应设备或检测能力的统一实现。
struct PaddleOcrEngine::Impl
{
    std::unique_ptr<PaddleOCR::OCRConfig> config;
    std::unique_ptr<PaddleOCR::DBDetector> detector;
    std::unique_ptr<PaddleOCR::Classifier> classifier;
    std::unique_ptr<PaddleOCR::CRNNRecognizer> recognizer;
};

// 函数说明：PaddleOcrEngine 构造函数创建组件并初始化其依赖和初始状态。
PaddleOcrEngine::PaddleOcrEngine(const QString &configPath)
    : m_impl(new Impl)
{
    m_impl->config.reset(
                new PaddleOCR::OCRConfig(configPath.toStdString()));
    m_impl->config->PrintConfigInfo();
    qDebug().noquote() << QString::fromWCharArray(
                    L"2. config.txt \u8bfb\u53d6\u5b8c\u6bd5");

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
    qDebug().noquote() << QString::fromWCharArray(
                    L"3. DBDetector \u6a21\u578b\u52a0\u8f7d"
                    L"\u5b8c\u6bd5");

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
        qDebug().noquote() << QString::fromWCharArray(
                        L"4. Classifier \u89d2\u5ea6\u5206\u7c7b"
                        L"\u6a21\u578b\u52a0\u8f7d\u5b8c\u6bd5");
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
    qDebug().noquote() << QString::fromWCharArray(
                    L"5. CRNNRecognizer \u6a21\u578b\u52a0\u8f7d"
                    L"\u5b8c\u6bd5");
}

PaddleOcrEngine::~PaddleOcrEngine() = default;

// 函数说明：recognize 函数实现名称所表示的处理步骤。
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
