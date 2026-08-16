#pragma once

#include "recipes/product_recipe.h"

#include <QVector>

#include <memory>
#include <vector>

#include <opencv2/core.hpp>

struct PreparedRecipeCharacterAsset
{
    QString fileName;
    QString normalizedBaseName;
    cv::Mat image;
};

struct PreparedRecipeProfile
{
    RecipeProfile definition;
    cv::Mat rawImage;
    cv::Mat trackingTemplate;
    cv::Mat stampRingTemplate;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
    std::vector<cv::Point2f> stampPolygon;
    std::vector<PreparedRecipeCharacterAsset> characterAssets;
    std::vector<cv::Mat> characterTemplates;
    std::vector<int> characterTemplateTargetIndexes;
};

struct PreparedRecipe
{
    ProductRecipeSnapshot recipe;
    QVector<PreparedRecipeProfile> profiles;
    TissueRecipeParameters tissue;
};

typedef std::shared_ptr<const PreparedRecipe> PreparedRecipeSnapshot;

QStringList preparedRecipeTargetUnits(const QString &targetText);
bool preparedRecipeCharacterAssetMatchesTarget(
    const QString &normalizedBaseName,
    const QString &target);

bool prepareRecipe(const ProductRecipe &recipe,
                   const QString &recipeDirectoryPath,
                   PreparedRecipeSnapshot *preparedRecipe,
                   QString *errorMessage = nullptr);
