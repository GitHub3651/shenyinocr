#include "detection/common/frame_preprocessor.h"

#include <opencv2/imgproc.hpp>

#include <vector>

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
