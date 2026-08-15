#pragma once

#include "appsettingsmanager.h"
#include "detection/common/character_template_matcher.h"
#include "recipes/product_recipe.h"
#include "recipes/template_profile_assets.h"

#include <QMap>
#include <QString>
#include <opencv2/core.hpp>
#include <vector>

struct WordTemplateProfile
{
    QString name;
    QString dirPath;
    cv::Mat trackingTemplate;
    std::vector<cv::Point2f> barcodePoly;
    std::vector<cv::Point2f> datePoly;
    TemplatePrivateSettings settings;
    RecipeProfile recipeProfile;
    TemplateProfileAssetManifest recipeAssetManifest;
    QMap<QString, QString> resolvedAssetPathsByRole;
    int targetCount = 0;
    std::vector<cv::Mat> digitTemplates;
    std::vector<int> digitTemplateTargetIndexes;
    TemplateMatchPreparedTemplates preparedDigitTemplates;
    mutable int preferredBarcodeStrategyId = -1;
    mutable unsigned int preferredBarcodeOptionFlags = 0;
    mutable int consecutiveBarcodeFailures = 0;
};
