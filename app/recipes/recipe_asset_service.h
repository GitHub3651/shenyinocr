#pragma once

#include "recipes/product_recipe.h"

#include <QImage>
#include <QMap>
#include <QString>

#include <opencv2/core.hpp>

#include <vector>

struct InitialRecipeProfileAssets
{
    int profileIndex = 0;
    cv::Mat rawImage;
    cv::Rect trackingImageRect;
    std::vector<cv::Point2f> stampPolygon;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
    cv::Mat stampRing;
};

// Owns editor-workspace resource encoding and recipe resource keys. It never
// commits the formal recipe directory; RecipeStore remains the only publisher.
class RecipeAssetService
{
public:
    bool stageInitialProfileAssets(
        const QString &workspacePath,
        const InitialRecipeProfileAssets &assets,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage) const;

    bool stageCharacterAssets(
        const QString &workspacePath,
        int profileIndex,
        const QMap<QString, QImage> &characterImages,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage) const;
};
