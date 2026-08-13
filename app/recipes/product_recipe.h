#ifndef RECIPES_PRODUCT_RECIPE_H
#define RECIPES_PRODUCT_RECIPE_H

#include <QJsonObject>
#include <QMap>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

enum class DetectionMode
{
    Stamp,
    Word,
    Ocr,
    Tissue,
    BarcodeWord
};

struct TissueRecipeParameters
{
    double roughnessThreshold = 6.0;
};

struct BarcodeRecipeParameters
{
    unsigned int formatMask = 1u;
    int roiPaddingPercent = 8;
    int maxDecodeTimeMs = 60;
    bool enableFallback = true;
};

struct RecipeCharacterBox
{
    QString name;
    QRect rect;
};

struct RecipeProfile
{
    QString name;
    QString targetText;
    double imageThreshold = 70.0;
    QRectF trackingBox;
    bool hasValidBoxes = false;
    QSize characterSourceImageSize = QSize(0, 0);
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

QString detectionModeId(DetectionMode mode);
bool detectionModeFromId(const QString &modeId, DetectionMode *mode);
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
