// 文件作用：本文件用于统一执行图像旋转、颜色通道选择和检测前的基础预处理。
// 主要职责：统一执行图像旋转、颜色通道选择和检测前的基础预处理。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "detection/common/frame_preprocessor.h"

#include <opencv2/imgproc.hpp>

#include <vector>

// 函数说明：transform 函数校验、转换或恢复对应数据。
bool FramePreprocessor::transform(
    const cv::Mat &source,
    const FramePreprocessSettings &settings,
    cv::Mat *output)
{
    if (!output || source.empty()) {
        return false;
    }
    *output = source.clone();
    switch (settings.rotation) {
    case FrameRotation::Clockwise90:
        cv::rotate(*output, *output, cv::ROTATE_90_CLOCKWISE);
        break;
    case FrameRotation::CounterClockwise90:
        cv::rotate(*output, *output, cv::ROTATE_90_COUNTERCLOCKWISE);
        break;
    case FrameRotation::Rotate180:
        cv::rotate(*output, *output, cv::ROTATE_180);
        break;
    case FrameRotation::None:
    default:
        break;
    }

    if (settings.colorChannel == FrameColorChannel::Color
            || output->channels() < 3) {
        return !output->empty();
    }
    std::vector<cv::Mat> channels;
    cv::split(*output, channels);
    switch (settings.colorChannel) {
    case FrameColorChannel::Red:
        *output = channels[2];
        break;
    case FrameColorChannel::Green:
        *output = channels[1];
        break;
    case FrameColorChannel::Blue:
        *output = channels[0];
        break;
    case FrameColorChannel::Color:
    default:
        break;
    }
    return !output->empty();
}
