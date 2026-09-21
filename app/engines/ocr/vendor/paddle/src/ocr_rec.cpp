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
        cv::Mat cropped = GetPerspectiveCropImage(image, box);
        if (cropped.rows != 0
                && static_cast<float>(cropped.rows)
                / static_cast<float>(cropped.cols) >= 1.5f) {
            cv::rotate(cropped, cropped, cv::ROTATE_90_COUNTERCLOCKWISE);
        }
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

std::string CRNNRecognizer::RunCharacter(const cv::Mat &image)
{
    const std::vector<std::vector<int>> box = {
        {0, 0},
        {image.cols - 1, 0},
        {image.cols - 1, image.rows - 1},
        {0, image.rows - 1}
    };
    const cv::Mat cropped = GetPerspectiveCropImage(image, box);
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
        const float *begin = &output[step * outputShape[2]];
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
    return text;
}

std::vector<RecLineResult> CRNNRecognizer::RunCharacters(
        const std::vector<std::vector<std::vector<int>>> &boxes,
        const cv::Mat &image)
{
    std::vector<RecLineResult> recognized;
    recognized.reserve(boxes.size());
    for (const std::vector<std::vector<int>> &box : boxes) {
        RecLineResult line;
        cv::Mat cropped = GetPerspectiveCropImage(image, box);
        if (cropped.rows != 0
                && static_cast<float>(cropped.rows)
                / static_cast<float>(cropped.cols) >= 1.5f) {
            cv::rotate(cropped, cropped, cv::ROTATE_90_COUNTERCLOCKWISE);
            line.rotated90 = true;
        }
        cv::Mat resizedImage;
        const int contentWidth = resize_.Run(cropped, resizedImage);
        const int targetWidth = resizedImage.cols;

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

        int activeCharacterIndex = 0;
        int firstStep = 0;
        int lastStep = 0;
        std::vector<float> centerRatios;
        const float effectiveSteps =
                static_cast<float>(outputShape[1])
                * static_cast<float>(contentWidth)
                / static_cast<float>(targetWidth);

        const auto appendCharacter = [&]() {
            if (activeCharacterIndex == 0) {
                return;
            }
            if (activeCharacterIndex
                    >= static_cast<int>(labels_.size())) {
                throw std::runtime_error(
                            "PP-OCRv6 tiny REC output does not match dictionary");
            }

            RecCharacterResult character;
            character.text = labels_[activeCharacterIndex];
            const float centerStep =
                    (firstStep + lastStep) * 0.5f;
            centerRatios.push_back(
                        std::max(
                            0.0f,
                            std::min(
                                (centerStep + 0.5f) / effectiveSteps,
                                1.0f)));
            line.characters.push_back(character);
        };

        for (int step = 0; step < outputShape[1]; ++step) {
            const float *begin =
                    &output[step * outputShape[2]];
            const int characterIndex = static_cast<int>(
                        Utility::Argmax(begin, begin + outputShape[2]));

            if (activeCharacterIndex > 0
                    && characterIndex != activeCharacterIndex) {
                appendCharacter();
                activeCharacterIndex = 0;
            }

            if (characterIndex > 0) {
                if (activeCharacterIndex == 0) {
                    activeCharacterIndex = characterIndex;
                    firstStep = step;
                }
                lastStep = step;
            }
        }
        appendCharacter();

        if (!line.characters.empty()) {
            cv::Mat gray;
            cv::cvtColor(cropped, gray, cv::COLOR_BGR2GRAY);
            cv::Mat foreground;
            cv::threshold(gray,
                          foreground,
                          0.0,
                          255.0,
                          cv::THRESH_BINARY | cv::THRESH_OTSU);
            if (cv::countNonZero(foreground)
                    > foreground.rows * foreground.cols / 2) {
                cv::bitwise_not(foreground, foreground);
            }

            std::vector<int> columnCounts(
                        static_cast<std::size_t>(foreground.cols), 0);
            for (int x = 0; x < foreground.cols; ++x) {
                columnCounts[static_cast<std::size_t>(x)] =
                        cv::countNonZero(foreground.col(x));
            }

            const int margin = std::max(
                        1,
                        cvRound(static_cast<float>(foreground.rows) * 0.03f));
            std::vector<int> boundaries(line.characters.size() + 1, 0);
            const int firstCenter = std::max(
                        0,
                        std::min(
                            cvRound(centerRatios.front()
                                    * static_cast<float>(foreground.cols - 1)),
                            foreground.cols - 1));
            int firstForeground = 0;
            while (firstForeground <= firstCenter
                   && columnCounts[static_cast<std::size_t>(
                       firstForeground)] == 0) {
                ++firstForeground;
            }
            boundaries.front() = firstForeground <= firstCenter
                    ? std::max(0, firstForeground - margin)
                    : 0;

            for (std::size_t index = 1;
                 index < line.characters.size();
                 ++index) {
                const int start = std::max(
                            0,
                            std::min(
                                cvRound(centerRatios[index - 1]
                                        * static_cast<float>(
                                            foreground.cols - 1)),
                                foreground.cols - 1));
                const int end = std::max(
                            start,
                            std::min(
                                cvRound(centerRatios[index]
                                        * static_cast<float>(
                                            foreground.cols - 1)),
                                foreground.cols - 1));
                const int midpoint = (start + end) / 2;
                int separator = start;
                int minimumCount = foreground.rows + 1;
                int minimumDistance = foreground.cols + 1;
                for (int x = start; x <= end; ++x) {
                    const int count = columnCounts[
                            static_cast<std::size_t>(x)];
                    const int distance = x >= midpoint
                            ? x - midpoint
                            : midpoint - x;
                    if (count < minimumCount
                            || (count == minimumCount
                                && distance < minimumDistance)) {
                        separator = x;
                        minimumCount = count;
                        minimumDistance = distance;
                    }
                }
                boundaries[index] = separator;
            }

            const int lastCenter = std::max(
                        0,
                        std::min(
                            cvRound(centerRatios.back()
                                    * static_cast<float>(foreground.cols - 1)),
                            foreground.cols - 1));
            int lastForeground = foreground.cols - 1;
            while (lastForeground >= lastCenter
                   && columnCounts[static_cast<std::size_t>(
                       lastForeground)] == 0) {
                --lastForeground;
            }
            boundaries.back() = lastForeground >= lastCenter
                    ? std::min(
                        foreground.cols,
                        lastForeground + 1 + margin)
                    : foreground.cols;

            for (std::size_t index = 0;
                 index < line.characters.size();
                 ++index) {
                const int left = std::min(
                            boundaries[index], foreground.cols - 1);
                const int right = std::max(
                            left + 1,
                            std::min(
                                boundaries[index + 1], foreground.cols));
                int top = 0;
                int bottom = foreground.rows;
                int firstRow = 0;
                while (firstRow < foreground.rows
                       && cv::countNonZero(
                           foreground.row(firstRow).colRange(left, right))
                          == 0) {
                    ++firstRow;
                }
                if (firstRow < foreground.rows) {
                    int lastRow = foreground.rows - 1;
                    while (lastRow > firstRow
                           && cv::countNonZero(
                               foreground.row(lastRow).colRange(left, right))
                              == 0) {
                        --lastRow;
                    }
                    top = std::max(0, firstRow - margin);
                    bottom = std::min(
                                foreground.rows,
                                lastRow + 1 + margin);
                }

                line.characters[index].normalizedRect = cv::Rect2f(
                            static_cast<float>(left)
                                / static_cast<float>(foreground.cols),
                            static_cast<float>(top)
                                / static_cast<float>(foreground.rows),
                            static_cast<float>(right - left)
                                / static_cast<float>(foreground.cols),
                            static_cast<float>(bottom - top)
                                / static_cast<float>(foreground.rows));
            }
        }
        recognized.push_back(line);
    }
    return recognized;
}

cv::Mat CRNNRecognizer::GetPerspectiveCropImage(
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
    return cropped;
}

} // namespace PaddleOCR
