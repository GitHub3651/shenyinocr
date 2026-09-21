#pragma once

#include "engines/ocr/vendor/paddle/include/preprocess_op.h"

#include <opencv2/core.hpp>
#include <paddle_inference_api.h>

#include <memory>
#include <string>
#include <vector>

namespace PaddleOCR {

struct RecCharacterResult
{
    std::string text;
    cv::Rect2f normalizedRect;
};

struct RecLineResult
{
    std::vector<RecCharacterResult> characters;
    bool rotated90 = false;
};

class CRNNRecognizer
{
public:
    CRNNRecognizer(const std::string &modelDirectory,
                   int cpuMathLibraryNumThreads,
                   bool useMkldnn,
                   const std::string &labelPath);

    std::vector<std::string> Run(
            const std::vector<std::vector<std::vector<int>>> &boxes,
            const cv::Mat &image);

    std::string RunCharacter(const cv::Mat &image);

    std::vector<RecLineResult> RunCharacters(
            const std::vector<std::vector<std::vector<int>>> &boxes,
            const cv::Mat &image);

private:
    void LoadModel(const std::string &modelDirectory);
    cv::Mat GetPerspectiveCropImage(
            const cv::Mat &image,
            const std::vector<std::vector<int>> &box) const;

    std::shared_ptr<paddle_infer::Predictor> predictor_;
    int cpuMathLibraryNumThreads_ = 4;
    bool useMkldnn_ = true;
    std::vector<std::string> labels_;

    RecResizeImg resize_;
    Permute permute_;
};

} // namespace PaddleOCR
