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

struct TissueRecipeParameters
{
    double roughnessThreshold = 6.0;
};

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

struct RecipeCharacterBox
{
    QString name;
    QRect rect;
};

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
