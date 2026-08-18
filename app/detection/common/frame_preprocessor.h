// 文件作用：本文件用于统一执行图像旋转、颜色通道选择和检测前的基础预处理。
// 主要职责：统一执行图像旋转、颜色通道选择和检测前的基础预处理。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <opencv2/core.hpp>

// 组件说明：FrameRotation 枚举列出该组件允许使用的稳定状态和选项。
enum class FrameRotation
{
    None,
    Clockwise90,
    CounterClockwise90,
    Rotate180
};

// 组件说明：FrameColorChannel 枚举列出该组件允许使用的稳定状态和选项。
enum class FrameColorChannel
{
    Color,
    Red,
    Green,
    Blue
};

// 组件说明：FramePreprocessSettings 组件集中描述相关配置、规则和运行参数。
struct FramePreprocessSettings
{
    FrameRotation rotation = FrameRotation::None;
    FrameColorChannel colorChannel = FrameColorChannel::Color;
};

// 组件说明：FramePreprocessor 组件封装本文件中与其名称对应的单一职责。
class FramePreprocessor
{
public:
    static bool transform(
        const cv::Mat &source,
        const FramePreprocessSettings &settings,
        cv::Mat *output);
};
