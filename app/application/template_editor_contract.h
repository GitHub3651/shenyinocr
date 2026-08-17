#pragma once

#include "contracts/barcode_parameter_defaults.h"
#include "contracts/detection_mode.h"
#include "recipes/prepared_recipe.h"

#include <QString>
#include <QVector>

#include <opencv2/core.hpp>

#include <vector>

struct TemplateBarcodeValidationOptions
{
    unsigned int formatMask =
            BarcodeParameterDefaults::FormatMask;
    int roiPaddingPercent =
            BarcodeParameterDefaults::RoiPaddingPercent;
    int maxDecodeTimeMs =
            BarcodeParameterDefaults::MaxDecodeTimeMs;
    bool enableFallback =
            BarcodeParameterDefaults::EnableFallback;
};

struct TemplateBarcodeValidationResult
{
    bool readable = false;
    QString text;
};

// Mutable editor DTO derived exclusively from a PreparedRecipe snapshot.
// It carries neither filesystem paths nor persistence ownership.
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

struct TemplateRecipeCatalogEntry
{
    QString recipeId;
    QString displayName;
    DetectionMode detectionMode = DetectionMode::Stamp;
    int profileCount = 0;
};

struct TemplateRecipeCatalogIssue
{
    QString directoryName;
    QString message;
};

struct TemplateRecipeCatalog
{
    QVector<TemplateRecipeCatalogEntry> recipes;
    QVector<TemplateRecipeCatalogIssue> invalidRecipes;
};
