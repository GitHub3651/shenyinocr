#ifndef RECIPES_PRODUCT_RECIPE_H
#define RECIPES_PRODUCT_RECIPE_H

#include <QJsonObject>
#include <QMap>
#include <QString>

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

struct ProductRecipe
{
    static const int CurrentSchemaVersion = 1;

    int schemaVersion = CurrentSchemaVersion;
    QString recipeId;
    QString displayName;
    DetectionMode detectionMode = DetectionMode::Stamp;
    TissueRecipeParameters tissueParameters;
    QMap<QString, QString> assets;
};

typedef std::shared_ptr<const ProductRecipe> ProductRecipeSnapshot;

QString detectionModeId(DetectionMode mode);
bool detectionModeFromId(const QString &modeId, DetectionMode *mode);

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
