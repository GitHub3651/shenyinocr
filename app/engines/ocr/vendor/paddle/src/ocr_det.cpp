#include "engines/ocr/vendor/paddle/include/ocr_det.h"

#include <functional>
#include <numeric>
#include <stdexcept>

namespace PaddleOCR {

DBDetector::DBDetector(const std::string &modelDirectory,
                       int cpuMathLibraryNumThreads,
                       bool useMkldnn,
                       int maxSideLen,
                       float detDbThresh,
                       float detDbBoxThresh,
                       float detDbUnclipRatio)
    : cpuMathLibraryNumThreads_(cpuMathLibraryNumThreads),
      useMkldnn_(useMkldnn),
      maxSideLen_(maxSideLen),
      detDbThresh_(detDbThresh),
      detDbBoxThresh_(detDbBoxThresh),
      detDbUnclipRatio_(detDbUnclipRatio)
{
    LoadModel(modelDirectory);
}

void DBDetector::LoadModel(const std::string &modelDirectory)
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
                    "Unable to create PP-OCRv6 tiny DET predictor");
    }
}

std::vector<std::vector<std::vector<int>>> DBDetector::Run(
        const cv::Mat &image)
{
    cv::Mat resizedImage;
    resize_.Run(image, resizedImage, maxSideLen_);
    normalize_.Run(
                &resizedImage,
                {0.485f, 0.456f, 0.406f},
                {1.0f / 0.229f,
                 1.0f / 0.224f,
                 1.0f / 0.225f});

    std::vector<float> input(
                3 * resizedImage.rows * resizedImage.cols,
                0.0f);
    permute_.Run(&resizedImage, input.data());

    const std::vector<std::string> inputNames = predictor_->GetInputNames();
    std::unique_ptr<paddle_infer::Tensor> inputTensor =
            predictor_->GetInputHandle(inputNames[0]);
    inputTensor->Reshape({1, 3, resizedImage.rows, resizedImage.cols});
    inputTensor->CopyFromCpu(input.data());
    if (!predictor_->Run()) {
        throw std::runtime_error(
                    "PP-OCRv6 tiny DET Predictor::Run() returned false");
    }

    const std::vector<std::string> outputNames = predictor_->GetOutputNames();
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

    const int outputHeight = outputShape[2];
    const int outputWidth = outputShape[3];
    cv::Mat prediction(
                outputHeight,
                outputWidth,
                CV_32FC1,
                output.data());
    const cv::Mat bitmap = prediction > detDbThresh_;
    return postProcessor_.BoxesFromBitmap(
                prediction,
                bitmap,
                image.cols,
                image.rows,
                detDbBoxThresh_,
                detDbUnclipRatio_);
}

} // namespace PaddleOCR
