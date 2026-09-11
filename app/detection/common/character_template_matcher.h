#ifndef DETECTION_COMMON_CHARACTER_TEMPLATE_MATCHER_H
#define DETECTION_COMMON_CHARACTER_TEMPLATE_MATCHER_H

#include <opencv2/core.hpp>

#include <cstddef>
#include <tuple>
#include <vector>

struct PreparedCharacterTemplates
{
    std::vector<cv::Mat> grayTemplates;
    std::vector<cv::Mat> smallTemplates;

    bool isValid() const
    {
        return !grayTemplates.empty()
                && grayTemplates.size() == smallTemplates.size();
    }
};

struct CharacterMatchResult
{
    int detectedCount = 0;
    std::vector<std::tuple<cv::Rect, double, size_t> > matches;
};

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
