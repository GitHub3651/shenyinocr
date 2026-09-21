#include "engines/ocr/vendor/paddle/include/preprocess_op.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace PaddleOCR {

void Normalize::Run(cv::Mat *image,
                    const std::vector<float> &mean,
                    const std::vector<float> &scale,
                    bool isScale) const
{
    const double inputScale = isScale ? 1.0 / 255.0 : 1.0;
    image->convertTo(*image, CV_32FC3, inputScale);
    for (int row = 0; row < image->rows; ++row) {
        for (int column = 0; column < image->cols; ++column) {
            cv::Vec3f &pixel = image->at<cv::Vec3f>(row, column);
            for (int channel = 0; channel < 3; ++channel) {
                pixel[channel] =
                        (pixel[channel] - mean[channel]) * scale[channel];
            }
        }
    }
}

void Permute::Run(const cv::Mat *image, float *data) const
{
    const int channelSize = image->rows * image->cols;
    for (int channel = 0; channel < image->channels(); ++channel) {
        cv::extractChannel(
                    *image,
                    cv::Mat(image->rows,
                            image->cols,
                            CV_32FC1,
                            data + channel * channelSize),
                    channel);
    }
}

void DetResizeImg::Run(const cv::Mat &image,
                       cv::Mat &resizedImage,
                       int maxSideLen) const
{
    cv::Mat input = image;
    if (image.rows + image.cols < 64) {
        const int paddedHeight = std::max(32, image.rows);
        const int paddedWidth = std::max(32, image.cols);
        input = cv::Mat::zeros(paddedHeight, paddedWidth, image.type());
        image.copyTo(input(cv::Rect(0, 0, image.cols, image.rows)));
    }

    const int inputHeight = input.rows;
    const int inputWidth = input.cols;
    float ratio = 1.0f;
    if (std::max(inputHeight, inputWidth) > maxSideLen) {
        ratio = static_cast<float>(maxSideLen)
                / static_cast<float>(std::max(inputHeight, inputWidth));
    }

    const int resizedHeight = std::max(
                static_cast<int>(
                    std::round(inputHeight * ratio / 32.0f) * 32),
                32);
    const int resizedWidth = std::max(
                static_cast<int>(
                    std::round(inputWidth * ratio / 32.0f) * 32),
                32);

    cv::resize(input,
               resizedImage,
               cv::Size(resizedWidth, resizedHeight));
}

int RecResizeImg::Run(const cv::Mat &image,
                      cv::Mat &resizedImage,
                      const std::vector<int> &imageShape) const
{
    const int channels = imageShape[0];
    const int targetHeight = imageShape[1];
    const int baseWidth = imageShape[2];
    const int maximumWidth = 3200;
    const float imageRatio = static_cast<float>(image.cols)
            / static_cast<float>(image.rows);
    const float baseRatio = static_cast<float>(baseWidth)
            / static_cast<float>(targetHeight);
    const int targetWidth = std::min(
                maximumWidth,
                static_cast<int>(targetHeight
                                 * std::max(baseRatio, imageRatio)));
    const int contentWidth = std::min(
                targetWidth,
                static_cast<int>(std::ceil(targetHeight * imageRatio)));

    cv::Mat resized;
    cv::resize(image,
               resized,
               cv::Size(contentWidth, targetHeight),
               0.0,
               0.0,
               cv::INTER_LINEAR);
    cv::Mat normalized;
    resized.convertTo(normalized, CV_32FC3, 1.0 / 127.5, -1.0);

    resizedImage = cv::Mat::zeros(
                targetHeight,
                targetWidth,
                CV_MAKETYPE(CV_32F, channels));
    normalized.copyTo(
                resizedImage(cv::Rect(0, 0, contentWidth, targetHeight)));
    return contentWidth;
}

} // namespace PaddleOCR
