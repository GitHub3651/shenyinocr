#pragma once

#include "recipes/prepared_recipe.h"

#include <QString>
#include <opencv2/core.hpp>
#include <vector>

// Mutable UI/runtime cache derived exclusively from a PreparedRecipe snapshot.
// It intentionally carries no filesystem path or persistence metadata.
struct WordTemplateProfile
{
    QString name;
    cv::Mat rawImage;
    cv::Mat trackingTemplate;
    std::vector<cv::Point2f> barcodePoly;
    std::vector<cv::Point2f> datePoly;
    RecipeProfile settings;
    int targetCount = 0;
    std::vector<PreparedRecipeCharacterAsset> characterAssets;
    std::vector<cv::Mat> digitTemplates;
    std::vector<int> digitTemplateTargetIndexes;
};
