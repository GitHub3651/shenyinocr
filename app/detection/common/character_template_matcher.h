#ifndef DETECTION_COMMON_CHARACTER_TEMPLATE_MATCHER_H
#define DETECTION_COMMON_CHARACTER_TEMPLATE_MATCHER_H

#include <opencv2/core.hpp>

#include <cstddef>
#include <tuple>
#include <vector>

struct TemplateMatchPreparedTemplates
{
    std::vector<cv::Mat> grayTemplates;
    std::vector<cv::Mat> smallTemplates;

    bool isValid() const
    {
        return !grayTemplates.empty()
                && grayTemplates.size() == smallTemplates.size();
    }
};

struct CharacterTemplateMatchResult
{
    int detectedCount = 0;
    std::vector<std::tuple<cv::Rect, double, size_t> > matches;
};

class CharacterTemplateMatcher
{
public:
    static TemplateMatchPreparedTemplates prepare(
        const std::vector<cv::Mat> &digitTemplates);

    static CharacterTemplateMatchResult match(
        const cv::Mat &targetImage,
        const TemplateMatchPreparedTemplates &preparedTemplates,
        const std::vector<int> &templateTargetIndexes,
        int thresholdPercent);
};

#endif // DETECTION_COMMON_CHARACTER_TEMPLATE_MATCHER_H
