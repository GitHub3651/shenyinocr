// 文件作用：本文件用于定义配方资源加载完成后的只读运行数据和校验结果。
// 主要职责：定义配方资源加载完成后的只读运行数据和校验结果。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "recipes/product_recipe.h"

#include <QVector>

#include <memory>
#include <vector>

#include <opencv2/core.hpp>

// 组件说明：PreparedRecipeCharacterAsset 数据结构集中保存该流程需要的一组相关数据。
struct PreparedRecipeCharacterAsset
{
    QString fileName;
    QString normalizedBaseName;
    cv::Mat image;
};

// 组件说明：PreparedRecipeProfile 数据结构集中保存该流程需要的一组相关数据。
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

// 组件说明：PreparedRecipe 数据结构集中保存该流程需要的一组相关数据。
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
