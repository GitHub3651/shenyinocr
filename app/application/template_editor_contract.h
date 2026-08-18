// 文件作用：本文件用于定义模板编辑页面与应用服务之间传递的命令和只读数据。
// 主要职责：定义模板编辑页面与应用服务之间传递的命令和只读数据。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、配方和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "contracts/barcode_parameter_defaults.h"
#include "contracts/detection_mode.h"
#include "recipes/prepared_recipe.h"

#include <QString>
#include <QVector>

#include <opencv2/core.hpp>

#include <vector>

// 组件说明：TemplateBarcodeValidationOptions 组件集中描述相关配置、规则和运行参数。
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

// 组件说明：TemplateBarcodeValidationResult 数据结构保存一次操作的结果、状态和错误信息。
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

// 组件说明：TemplateRecipeCatalogEntry 数据结构集中保存该流程需要的一组相关数据。
struct TemplateRecipeCatalogEntry
{
    QString recipeId;
    QString displayName;
    DetectionMode detectionMode = DetectionMode::Stamp;
    int profileCount = 0;
};

// 组件说明：TemplateRecipeCatalogIssue 数据结构集中保存该流程需要的一组相关数据。
struct TemplateRecipeCatalogIssue
{
    QString directoryName;
    QString message;
};

// 组件说明：TemplateRecipeCatalog 数据结构集中保存该流程需要的一组相关数据。
struct TemplateRecipeCatalog
{
    QVector<TemplateRecipeCatalogEntry> recipes;
    QVector<TemplateRecipeCatalogIssue> invalidRecipes;
};
