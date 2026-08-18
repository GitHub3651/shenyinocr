// 文件作用：本文件用于定义产品配方、Profile、ROI和五种模式参数的正式数据模型。
// 主要职责：定义产品配方、Profile、ROI和五种模式参数的正式数据模型。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef RECIPES_PRODUCT_RECIPE_H
#define RECIPES_PRODUCT_RECIPE_H

#include "contracts/barcode_parameter_defaults.h"
#include "contracts/detection_mode.h"

#include <QJsonObject>
#include <QMap>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

// 组件说明：TissueRecipeParameters 数据结构集中保存该流程需要的一组相关数据。
struct TissueRecipeParameters
{
    double roughnessThreshold = 6.0;
};

// 组件说明：BarcodeRecipeParameters 数据结构集中保存该流程需要的一组相关数据。
struct BarcodeRecipeParameters
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

// 组件说明：RecipeCharacterBox 数据结构集中保存该流程需要的一组相关数据。
struct RecipeCharacterBox
{
    QString name;
    QRect rect;
};

// 组件说明：RecipeProfile 数据结构集中保存该流程需要的一组相关数据。
struct RecipeProfile
{
    static const int DefaultImageThresholdPercent = 70;

    QString name;
    QString targetText;
    int imageThresholdPercent = DefaultImageThresholdPercent;
    QRectF trackingRoi;
    QSize characterSourceSize = QSize(0, 0);
    QVector<RecipeCharacterBox> characterBoxes;
    BarcodeRecipeParameters barcodeParameters;
    QMap<QString, QString> assetKeys;
};

// 组件说明：ProductRecipe 数据结构集中保存该流程需要的一组相关数据。
struct ProductRecipe
{
    static const int CurrentSchemaVersion = 1;

    int schemaVersion = CurrentSchemaVersion;
    QString recipeId;
    QString displayName;
    DetectionMode detectionMode = DetectionMode::Stamp;
    TissueRecipeParameters tissueParameters;
    QVector<RecipeProfile> profiles;
    QMap<QString, QString> assets;
};

typedef std::shared_ptr<const ProductRecipe> ProductRecipeSnapshot;

bool isTemplateRecipeMode(DetectionMode mode);
QStringList requiredTemplateProfileAssetRoles(DetectionMode mode);

ProductRecipe createProductRecipe(const QString &displayName,
                                  DetectionMode detectionMode);

bool validateProductRecipe(const ProductRecipe &recipe,
                           QString *errorMessage = nullptr);

QJsonObject productRecipeToJson(const ProductRecipe &recipe);
bool productRecipeFromJson(const QJsonObject &json,
                           ProductRecipe *recipe,
                           QString *errorMessage = nullptr);

ProductRecipeSnapshot makeProductRecipeSnapshot(
        const ProductRecipe &recipe,
        QString *errorMessage = nullptr);

#endif // RECIPES_PRODUCT_RECIPE_H
