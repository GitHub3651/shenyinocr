#pragma once

#include "engines/ocr/vendor/paddle/include/postprocess_op.h"
#include "engines/ocr/vendor/paddle/include/preprocess_op.h"

#include <opencv2/core.hpp>
#include <paddle_inference_api.h>

#include <memory>
#include <string>
#include <vector>

namespace PaddleOCR {

class DBDetector
{
public:
    DBDetector(const std::string &modelDirectory,
               int cpuMathLibraryNumThreads,
               bool useMkldnn,
               int maxSideLen,
               float detDbThresh,
               float detDbBoxThresh,
               float detDbUnclipRatio);

    std::vector<std::vector<std::vector<int>>> Run(const cv::Mat &image);

private:
    void LoadModel(const std::string &modelDirectory);

    std::shared_ptr<paddle_infer::Predictor> predictor_;
    int cpuMathLibraryNumThreads_ = 4;
    bool useMkldnn_ = true;
    int maxSideLen_ = 960;
    float detDbThresh_ = 0.2f;
    float detDbBoxThresh_ = 0.4f;
    float detDbUnclipRatio_ = 1.4f;

    DetResizeImg resize_;
    Normalize normalize_;
    Permute permute_;
    PostProcessor postProcessor_;
};

} // namespace PaddleOCR
