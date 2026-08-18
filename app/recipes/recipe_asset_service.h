// 文件作用：本文件用于管理模板图、字符图和校准文件等配方资源的读取与写入。
// 主要职责：管理模板图、字符图和校准文件等配方资源的读取与写入。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "recipes/product_recipe.h"

#include <QImage>
#include <QMap>
#include <QString>

#include <opencv2/core.hpp>

#include <vector>

// 组件说明：InitialRecipeProfileAssets 数据结构集中保存该流程需要的一组相关数据。
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
