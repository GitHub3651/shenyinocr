#include "character_template_matcher.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace {

double calculateIou(const cv::Rect &rectA, const cv::Rect &rectB)
{
    const cv::Rect intersection = rectA & rectB;
    if (intersection.area() <= 0) {
        return 0.0;
    }
    const double unionArea = rectA.area() + rectB.area()
            - intersection.area();
    return unionArea > 0.0
            ? intersection.area() / unionArea
            : 0.0;
}

} // namespace

PreparedCharacterTemplates CharacterGlyphMatcher::prepare(
    const std::vector<cv::Mat> &digitTemplates)
{
    PreparedCharacterTemplates prepared;
    prepared.grayTemplates.reserve(digitTemplates.size());
    prepared.smallTemplates.reserve(digitTemplates.size());

    const double scale = 0.5;
    for (const cv::Mat &digitTemplate : digitTemplates) {
        cv::Mat grayTemplate;
        if (digitTemplate.empty()) {
            prepared.grayTemplates.emplace_back();
            prepared.smallTemplates.emplace_back();
            continue;
        }

        if (digitTemplate.channels() == 3) {
            cv::cvtColor(digitTemplate, grayTemplate, cv::COLOR_BGR2GRAY);
        } else if (digitTemplate.channels() == 4) {
            cv::cvtColor(digitTemplate, grayTemplate, cv::COLOR_BGRA2GRAY);
        } else {
            grayTemplate = digitTemplate.clone();
        }

        cv::Mat smallTemplate;
        if (!grayTemplate.empty()) {
            cv::resize(grayTemplate,
                       smallTemplate,
                       cv::Size(),
                       scale,
                       scale,
                       cv::INTER_LINEAR);
        }
        prepared.grayTemplates.push_back(grayTemplate);
        prepared.smallTemplates.push_back(smallTemplate);
    }

    return prepared;
}

CharacterMatchResult CharacterGlyphMatcher::match(
    const cv::Mat &targetImage,
    const PreparedCharacterTemplates &preparedTemplates,
    const std::vector<int> &templateTargetIndexes,
    int thresholdPercent)
{
    CharacterMatchResult result;
    if (!preparedTemplates.isValid() || targetImage.empty()) {
        return result;
    }

    cv::Mat targetGrayImage;
    cv::cvtColor(targetImage, targetGrayImage, cv::COLOR_BGR2GRAY);

    const std::vector<cv::Mat> &grayTemplates =
            preparedTemplates.grayTemplates;
    const std::vector<cv::Mat> &smallTemplates =
            preparedTemplates.smallTemplates;

    std::vector<int> effectiveTargetIndexes;
    effectiveTargetIndexes.reserve(grayTemplates.size());
    if (templateTargetIndexes.size() == grayTemplates.size()) {
        effectiveTargetIndexes.assign(templateTargetIndexes.begin(),
                                      templateTargetIndexes.end());
    } else {
        for (int i = 0; i < static_cast<int>(grayTemplates.size()); ++i) {
            effectiveTargetIndexes.push_back(i);
        }
    }

    const double threshold = static_cast<double>(thresholdPercent) / 100.0;
    const double scale = 0.5;
    cv::Mat smallTarget;
    cv::resize(targetGrayImage,
               smallTarget,
               cv::Size(),
               scale,
               scale,
               cv::INTER_LINEAR);

    std::vector<cv::Mat> matchScoreMaps(grayTemplates.size());
    cv::parallel_for_(
                cv::Range(0, static_cast<int>(grayTemplates.size())),
                [&](const cv::Range &range) {
        for (int i = range.start; i < range.end; ++i) {
            const cv::Mat &smallTemplate =
                    smallTemplates[static_cast<size_t>(i)];
            if (smallTemplate.empty()
                    || smallTarget.cols < smallTemplate.cols
                    || smallTarget.rows < smallTemplate.rows) {
                continue;
            }
            cv::matchTemplate(smallTarget,
                              smallTemplate,
                              matchScoreMaps[static_cast<size_t>(i)],
                              cv::TM_CCOEFF_NORMED);
        }
    });

    int targetGroupCount = 0;
    for (int targetIndex : effectiveTargetIndexes) {
        if (targetIndex >= 0) {
            targetGroupCount = std::max(targetGroupCount,
                                        targetIndex + 1);
        }
    }
    if (targetGroupCount <= 0) {
        targetGroupCount = static_cast<int>(grayTemplates.size());
    }

    std::vector<std::vector<size_t> > templateIndexesByTarget(
                static_cast<size_t>(targetGroupCount));
    for (size_t i = 0; i < effectiveTargetIndexes.size(); ++i) {
        const int targetIndex = effectiveTargetIndexes[i];
        if (targetIndex >= 0 && targetIndex < targetGroupCount) {
            templateIndexesByTarget[static_cast<size_t>(targetIndex)]
                    .push_back(i);
        }
    }

    std::vector<std::vector<cv::Rect> > allMatchLocations(
                grayTemplates.size());
    std::vector<std::vector<double> > allMatchScores(
                grayTemplates.size());
    std::vector<cv::Rect> selectedLocations;

    for (int targetIndex = 0;
         targetIndex < targetGroupCount;
         ++targetIndex) {
        bool found = false;
        double bestScore = threshold - 1.0;
        cv::Rect bestRect;
        size_t bestTemplateIndex = 0;

        for (size_t templateIndex :
             templateIndexesByTarget[static_cast<size_t>(targetIndex)]) {
            const cv::Mat &scoreMap = matchScoreMaps[templateIndex];
            const cv::Mat &originalTemplate = grayTemplates[templateIndex];
            if (scoreMap.empty() || originalTemplate.empty()) {
                continue;
            }

            for (int y = 0; y < scoreMap.rows; ++y) {
                const float *scoreRow = scoreMap.ptr<float>(y);
                for (int x = 0; x < scoreMap.cols; ++x) {
                    const double score = scoreRow[x];
                    if (score < threshold || score <= bestScore) {
                        continue;
                    }

                    const cv::Rect candidateRect(
                                static_cast<int>(std::round(x / scale)),
                                static_cast<int>(std::round(y / scale)),
                                originalTemplate.cols,
                                originalTemplate.rows);
                    bool overlapsSelected = false;
                    for (const cv::Rect &selectedRect : selectedLocations) {
                        if (calculateIou(candidateRect, selectedRect) > 0.3) {
                            overlapsSelected = true;
                            break;
                        }
                    }
                    if (overlapsSelected) {
                        continue;
                    }

                    bestScore = score;
                    bestRect = candidateRect;
                    bestTemplateIndex = templateIndex;
                    found = true;
                }
            }
        }

        if (found) {
            allMatchLocations[bestTemplateIndex].push_back(bestRect);
            allMatchScores[bestTemplateIndex].push_back(bestScore);
            selectedLocations.push_back(bestRect);
        }
    }

    double totalAverageWidth = 0.0;
    double totalAverageHeight = 0.0;
    int validTemplateCount = 0;
    for (const cv::Mat &digitTemplate : grayTemplates) {
        if (!digitTemplate.empty()
                && digitTemplate.rows > 0
                && digitTemplate.cols > 0) {
            totalAverageWidth += digitTemplate.cols;
            totalAverageHeight += digitTemplate.rows;
            ++validTemplateCount;
        }
    }

    const int globalAverageWidth = std::max(
                1,
                static_cast<int>(std::round(
                    totalAverageWidth
                    / (validTemplateCount > 0 ? validTemplateCount : 1))));
    const int globalAverageHeight = std::max(
                1,
                static_cast<int>(std::round(
                    totalAverageHeight
                    / (validTemplateCount > 0 ? validTemplateCount : 1))));
    for (std::vector<cv::Rect> &locations : allMatchLocations) {
        for (cv::Rect &rect : locations) {
            const cv::Point center = rect.tl()
                    + cv::Point(rect.width / 2, rect.height / 2);
            rect = cv::Rect(center.x - globalAverageWidth / 2,
                            center.y - globalAverageHeight / 2,
                            globalAverageWidth,
                            globalAverageHeight);
        }
    }

    std::vector<std::tuple<cv::Rect, double, size_t> > sortedMatches;
    for (size_t i = 0; i < allMatchLocations.size(); ++i) {
        if (allMatchLocations[i].empty()) {
            continue;
        }
        const int targetIndex = i < effectiveTargetIndexes.size()
                ? effectiveTargetIndexes[i]
                : static_cast<int>(i);
        if (targetIndex < 0) {
            continue;
        }
        sortedMatches.emplace_back(allMatchLocations[i][0],
                                   allMatchScores[i][0],
                                   static_cast<size_t>(targetIndex));
    }
    std::sort(sortedMatches.begin(),
              sortedMatches.end(),
              [](const std::tuple<cv::Rect, double, size_t> &left,
                 const std::tuple<cv::Rect, double, size_t> &right) {
        const cv::Rect &leftRect = std::get<0>(left);
        const cv::Rect &rightRect = std::get<0>(right);
        return leftRect.y < rightRect.y
                || (leftRect.y == rightRect.y
                    && leftRect.x < rightRect.x);
    });

    result.matches.swap(sortedMatches);
    for (const std::vector<cv::Rect> &locations : allMatchLocations) {
        result.detectedCount += static_cast<int>(locations.size());
    }
    return result;
}
