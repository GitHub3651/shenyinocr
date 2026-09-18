#include "paddle_ocr_engine.h"

#include "engines/ocr/vendor/paddle/include/config.h"
#include "engines/ocr/vendor/paddle/include/ocr_det.h"
#include "engines/ocr/vendor/paddle/include/ocr_rec.h"
#include "engines/ocr/vendor/paddle/include/utility.h"

#include "system_support/logging/log_categories.h"

struct PaddleOcrEngine::Impl
{
    std::unique_ptr<PaddleOCR::DBDetector> detector;
    std::unique_ptr<PaddleOCR::CRNNRecognizer> recognizer;
};

PaddleOcrEngine::PaddleOcrEngine(const QString &configPath)
    : m_impl(new Impl)
{
    const PaddleOCR::OCRConfig config(configPath.toStdString());
    m_impl->detector.reset(
                new PaddleOCR::DBDetector(
                    config.detModelDir,
                    config.cpuMathLibraryNumThreads,
                    config.useMkldnn,
                    config.maxSideLen,
                    config.detDbThresh,
                    config.detDbBoxThresh,
                    config.detDbUnclipRatio));

    m_impl->recognizer.reset(
                new PaddleOCR::CRNNRecognizer(
                    config.recModelDir,
                    config.cpuMathLibraryNumThreads,
                    config.useMkldnn,
                    config.charListFile));

    const cv::Mat warmupImage = cv::Mat::zeros(48, 320, CV_8UC3);
    m_impl->detector->Run(warmupImage);
    const std::vector<std::vector<std::vector<int>>> warmupBoxes = {
        {{0, 0}, {319, 0}, {319, 47}, {0, 47}}
    };
    m_impl->recognizer->Run(warmupBoxes, warmupImage);

    qCInfo(logDevice).noquote()
            << QStringLiteral(
                "event=ocr.engine_loaded model=PP-OCRv6_tiny_det+rec config=%1")
               .arg(configPath);
}

PaddleOcrEngine::~PaddleOcrEngine() = default;

std::vector<OcrRecognitionItem> PaddleOcrEngine::recognize(cv::Mat &image)
{
    const std::vector<std::vector<std::vector<int>>> boxes =
            PaddleOCR::Utility::SortQuadBoxes(
                m_impl->detector->Run(image));
    const std::vector<std::string> texts =
            m_impl->recognizer->Run(boxes, image);
    std::vector<OcrRecognitionItem> result;
    result.reserve(boxes.size());
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        OcrRecognitionItem item;
        item.text = i < texts.size() ? texts[i] : std::string();
        for (const std::vector<int> &point : boxes[i]) {
            item.box.emplace_back(point[0], point[1]);
        }
        result.push_back(item);
    }
    return result;
}
