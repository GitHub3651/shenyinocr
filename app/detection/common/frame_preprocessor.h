#pragma once

#include <opencv2/core.hpp>

enum class FrameRotation
{
    None,
    Clockwise90,
    CounterClockwise90,
    Rotate180
};

enum class FrameColorChannel
{
    Color,
    Red,
    Green,
    Blue
};

struct FramePreprocessSettings
{
    FrameRotation rotation = FrameRotation::None;
    FrameColorChannel colorChannel = FrameColorChannel::Color;
};

class FramePreprocessor
{
public:
    static bool transform(
        const cv::Mat &source,
        const FramePreprocessSettings &settings,
        cv::Mat *output);
};
