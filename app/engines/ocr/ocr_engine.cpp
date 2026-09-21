#include "engines/ocr/ocr_engine.h"

#include "engines/ocr/vendor/paddle/include/config.h"
#include "engines/ocr/vendor/paddle/include/ocr_det.h"
#include "engines/ocr/vendor/paddle/include/ocr_rec.h"
#include "engines/ocr/vendor/paddle/include/utility.h"
#include "system_support/logging/log_categories.h"

struct IOcrEngine::Impl
{
    std::unique_ptr<PaddleOCR::DBDetector> detector;
    std::unique_ptr<PaddleOCR::CRNNRecognizer> recognizer;
};

IOcrEngine::IOcrEngine(const QString &configPath)
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

IOcrEngine::~IOcrEngine() = default;

std::vector<OcrRecognitionItem> IOcrEngine::recognize(cv::Mat &image)
{
    const std::vector<std::vector<std::vector<int>>> boxes =
            PaddleOCR::Utility::SortQuadBoxes(
                m_impl->detector->Run(image));
    const std::vector<std::string> texts =
            m_impl->recognizer->Run(boxes, image);

    std::vector<OcrRecognitionItem> result;
    result.reserve(boxes.size());
    for (std::size_t index = 0; index < boxes.size(); ++index) {
        OcrRecognitionItem item;
        item.text = index < texts.size() ? texts[index] : std::string();
        for (const std::vector<int> &point : boxes[index]) {
            item.box.emplace_back(point[0], point[1]);
        }
        result.push_back(item);
    }
    return result;
}

std::string IOcrEngine::recognizeCharacter(cv::Mat &image)
{
    return m_impl->recognizer->RunCharacter(image);
}

std::vector<OcrRecognitionItem> IOcrEngine::segmentCharacters(
        cv::Mat &image)
{
    const std::vector<std::vector<std::vector<int>>> boxes =
            PaddleOCR::Utility::SortQuadBoxes(
                m_impl->detector->Run(image));
    const std::vector<PaddleOCR::RecLineResult> lines =
            m_impl->recognizer->RunCharacters(boxes, image);

    std::vector<OcrRecognitionItem> result;
    for (std::size_t lineIndex = 0;
         lineIndex < lines.size();
         ++lineIndex) {
        const PaddleOCR::RecLineResult &line = lines[lineIndex];
        const std::vector<std::vector<int>> &box = boxes[lineIndex];
        const cv::Point2f p0(
                    static_cast<float>(box[0][0]),
                    static_cast<float>(box[0][1]));
        const cv::Point2f p1(
                    static_cast<float>(box[1][0]),
                    static_cast<float>(box[1][1]));
        const cv::Point2f p2(
                    static_cast<float>(box[2][0]),
                    static_cast<float>(box[2][1]));
        const cv::Point2f p3(
                    static_cast<float>(box[3][0]),
                    static_cast<float>(box[3][1]));

        for (const PaddleOCR::RecCharacterResult &character :
             line.characters) {
            const float left = character.normalizedRect.x;
            const float top = character.normalizedRect.y;
            const float right = left + character.normalizedRect.width;
            const float bottom = top + character.normalizedRect.height;
            const auto mapPoint = [&](float horizontal, float vertical) {
                if (line.rotated90) {
                    const cv::Point2f leftEdge =
                            p0 + (p3 - p0) * horizontal;
                    const cv::Point2f rightEdge =
                            p1 + (p2 - p1) * horizontal;
                    return rightEdge + (leftEdge - rightEdge) * vertical;
                }
                const cv::Point2f topEdge =
                        p0 + (p1 - p0) * horizontal;
                const cv::Point2f bottomEdge =
                        p3 + (p2 - p3) * horizontal;
                return topEdge + (bottomEdge - topEdge) * vertical;
            };

            const cv::Point2f topLeft = mapPoint(left, top);
            const cv::Point2f topRight = mapPoint(right, top);
            const cv::Point2f bottomRight = mapPoint(right, bottom);
            const cv::Point2f bottomLeft = mapPoint(left, bottom);

            OcrRecognitionItem item;
            item.text = character.text;
            item.box = {
                cv::Point(cvRound(topLeft.x), cvRound(topLeft.y)),
                cv::Point(cvRound(topRight.x), cvRound(topRight.y)),
                cv::Point(cvRound(bottomRight.x), cvRound(bottomRight.y)),
                cv::Point(cvRound(bottomLeft.x), cvRound(bottomLeft.y))
            };
            result.push_back(item);
        }
    }
    return result;
}
