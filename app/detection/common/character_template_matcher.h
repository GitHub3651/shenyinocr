// 文件作用：本文件用于加载字符模板并完成字符区域匹配、评分和识别结果整理。
// 主要职责：加载字符模板并完成字符区域匹配、评分和识别结果整理。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DETECTION_COMMON_CHARACTER_TEMPLATE_MATCHER_H
#define DETECTION_COMMON_CHARACTER_TEMPLATE_MATCHER_H

#include <opencv2/core.hpp>

#include <cstddef>
#include <tuple>
#include <vector>

// 组件说明：PreparedCharacterTemplates 数据结构集中保存该流程需要的一组相关数据。
struct PreparedCharacterTemplates
{
    std::vector<cv::Mat> grayTemplates;
    std::vector<cv::Mat> smallTemplates;

    // 函数说明：isValid 函数检查相关状态并返回判断结果。
    bool isValid() const
    {
        return !grayTemplates.empty()
                && grayTemplates.size() == smallTemplates.size();
    }
};

// 组件说明：CharacterMatchResult 数据结构保存一次操作的结果、状态和错误信息。
struct CharacterMatchResult
{
    int detectedCount = 0;
    std::vector<std::tuple<cv::Rect, double, size_t> > matches;
};

// 组件说明：CharacterGlyphMatcher 组件提供对应设备或检测能力的统一实现。
class CharacterGlyphMatcher
{
public:
    static PreparedCharacterTemplates prepare(
        const std::vector<cv::Mat> &digitTemplates);

    static CharacterMatchResult match(
        const cv::Mat &targetImage,
        const PreparedCharacterTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes,
        int thresholdPercent);
};

#endif // DETECTION_COMMON_CHARACTER_TEMPLATE_MATCHER_H
