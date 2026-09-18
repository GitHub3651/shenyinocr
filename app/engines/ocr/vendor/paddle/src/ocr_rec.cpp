#include "engines/ocr/vendor/paddle/include/ocr_rec.h"

#include "engines/ocr/vendor/paddle/include/utility.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <functional>
#include <numeric>
#include <stdexcept>

namespace PaddleOCR {

CRNNRecognizer::CRNNRecognizer(const std::string &modelDirectory,
                               int cpuMathLibraryNumThreads,
                               bool useMkldnn,
                               const std::string &labelPath)
    : cpuMathLibraryNumThreads_(cpuMathLibraryNumThreads),
      useMkldnn_(useMkldnn),
      labels_(Utility::ReadDict(labelPath))
{
    labels_.insert(labels_.begin(), "blank");
    labels_.push_back(" ");
    LoadModel(modelDirectory);
}

void CRNNRecognizer::LoadModel(const std::string &modelDirectory)
{
    paddle_infer::Config config;
    config.SetModel(modelDirectory + "/inference.json",
                    modelDirectory + "/inference.pdiparams");
    config.DisableGpu();
    if (useMkldnn_) {
        config.EnableMKLDNN();
        config.SetMkldnnCacheCapacity(10);
    }
    config.SetCpuMathLibraryNumThreads(cpuMathLibraryNumThreads_);
    config.SwitchUseFeedFetchOps(false);
    config.SwitchSpecifyInputNames(true);
    config.SwitchIrOptim(true);
    config.DisableGlogInfo();

    predictor_ = paddle_infer::CreatePredictor(config);
    if (!predictor_) {
        throw std::runtime_error(
                    "Unable to create PP-OCRv6 tiny REC predictor");
    }
}

std::vector<std::string> CRNNRecognizer::Run(
        const std::vector<std::vector<std::vector<int>>> &boxes,
        const cv::Mat &image)
{
    std::vector<std::string> recognized;
    recognized.reserve(boxes.size());
    for (const std::vector<std::vector<int>> &box : boxes) {
        const cv::Mat cropped = GetRotateCropImage(image, box);
        cv::Mat resizedImage;
        resize_.Run(cropped, resizedImage);

        std::vector<float> input(
                    3 * resizedImage.rows * resizedImage.cols,
                    0.0f);
        permute_.Run(&resizedImage, input.data());

        const std::vector<std::string> inputNames =
                predictor_->GetInputNames();
        std::unique_ptr<paddle_infer::Tensor> inputTensor =
                predictor_->GetInputHandle(inputNames[0]);
        inputTensor->Reshape(
                    {1, 3, resizedImage.rows, resizedImage.cols});
        inputTensor->CopyFromCpu(input.data());
        if (!predictor_->Run()) {
            throw std::runtime_error(
                        "PP-OCRv6 tiny REC Predictor::Run() returned false");
        }

        const std::vector<std::string> outputNames =
                predictor_->GetOutputNames();
        std::unique_ptr<paddle_infer::Tensor> outputTensor =
                predictor_->GetOutputHandle(outputNames[0]);
        const std::vector<int> outputShape = outputTensor->shape();
        const int outputCount = std::accumulate(
                    outputShape.begin(),
                    outputShape.end(),
                    1,
                    std::multiplies<int>());
        std::vector<float> output(outputCount);
        outputTensor->CopyToCpu(output.data());

        std::string text;
        int previousIndex = 0;
        for (int step = 0; step < outputShape[1]; ++step) {
            const float *begin =
                    &output[step * outputShape[2]];
            const int characterIndex = static_cast<int>(
                        Utility::Argmax(begin, begin + outputShape[2]));
            if (characterIndex > 0
                    && !(step > 0 && characterIndex == previousIndex)) {
                if (characterIndex >= static_cast<int>(labels_.size())) {
                    throw std::runtime_error(
                                "PP-OCRv6 tiny REC output does not match dictionary");
                }
                text += labels_[characterIndex];
            }
            previousIndex = characterIndex;
        }
        recognized.push_back(text);
    }
    return recognized;
}

cv::Mat CRNNRecognizer::GetRotateCropImage(
        const cv::Mat &image,
        const std::vector<std::vector<int>> &box) const
{
    const std::vector<cv::Point2f> points = {
        cv::Point2f(static_cast<float>(box[0][0]),
                    static_cast<float>(box[0][1])),
        cv::Point2f(static_cast<float>(box[1][0]),
                    static_cast<float>(box[1][1])),
        cv::Point2f(static_cast<float>(box[2][0]),
                    static_cast<float>(box[2][1])),
        cv::Point2f(static_cast<float>(box[3][0]),
                    static_cast<float>(box[3][1]))
    };
    const float width = static_cast<float>(std::max(
                cv::norm(points[0] - points[1]),
                cv::norm(points[2] - points[3])));
    const float height = static_cast<float>(std::max(
                cv::norm(points[0] - points[3]),
                cv::norm(points[1] - points[2])));
    const int outputWidth = std::max(1, static_cast<int>(width));
    const int outputHeight = std::max(1, static_cast<int>(height));
    const std::vector<cv::Point2f> destination = {
        cv::Point2f(0.0f, 0.0f),
        cv::Point2f(static_cast<float>(outputWidth - 1), 0.0f),
        cv::Point2f(static_cast<float>(outputWidth - 1),
                    static_cast<float>(outputHeight - 1)),
        cv::Point2f(0.0f, static_cast<float>(outputHeight - 1))
    };

    const cv::Mat transform =
            cv::getPerspectiveTransform(points, destination);
    cv::Mat cropped;
    cv::warpPerspective(image,
                        cropped,
                        transform,
                        cv::Size(outputWidth, outputHeight),
                        cv::INTER_CUBIC,
                        cv::BORDER_REPLICATE);
    if (cropped.rows != 0
            && static_cast<float>(cropped.rows)
            / static_cast<float>(cropped.cols) >= 1.5f) {
        cv::rotate(cropped, cropped, cv::ROTATE_90_COUNTERCLOCKWISE);
    }
    return cropped;
}

} // namespace PaddleOCR
