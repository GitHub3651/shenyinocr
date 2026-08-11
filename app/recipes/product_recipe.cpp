#include "product_recipe.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonValue>
#include <QUuid>

#include <cmath>

const int ProductRecipe::CurrentSchemaVersion;

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool isCanonicalRecipeId(const QString &recipeId)
{
    const QString trimmed = recipeId.trimmed();
    const QUuid uuid(trimmed);
    return !uuid.isNull()
            && uuid.toString(QUuid::WithoutBraces) == trimmed.toLower();
}

bool isValidAssetPath(const QString &assetPath)
{
    const QString normalized = QDir::fromNativeSeparators(assetPath.trimmed());
    if (normalized.isEmpty()
            || normalized.contains(QLatin1Char(':'))
            || QDir::isAbsolutePath(normalized)
            || QFileInfo(normalized).isAbsolute()) {
        return false;
    }

    const QString cleaned = QDir::cleanPath(normalized);
    return cleaned.startsWith(QLatin1String("assets/"))
            && cleaned != QLatin1String("assets/..")
            && !cleaned.startsWith(QLatin1String("assets/../"));
}

bool readRequiredString(const QJsonObject &json,
                        const char *key,
                        QString *value,
                        QString *errorMessage)
{
    const QJsonValue jsonValue = json.value(QLatin1String(key));
    if (!jsonValue.isString()) {
        setError(errorMessage,
                 QStringLiteral("Recipe field is missing or is not a string: %1")
                 .arg(QLatin1String(key)));
        return false;
    }

    *value = jsonValue.toString();
    return true;
}

} // namespace

QString detectionModeId(DetectionMode mode)
{
    switch (mode) {
    case DetectionMode::Stamp:
        return QStringLiteral("stamp_detection");
    case DetectionMode::Word:
        return QStringLiteral("word_detection");
    case DetectionMode::Ocr:
        return QStringLiteral("ocr_detection");
    case DetectionMode::Tissue:
        return QStringLiteral("tissue_detection");
    case DetectionMode::BarcodeWord:
        return QStringLiteral("barcode_word_detection");
    }
    return QString();
}

bool detectionModeFromId(const QString &modeId, DetectionMode *mode)
{
    if (!mode) {
        return false;
    }

    const QString normalized = modeId.trimmed();
    if (normalized == QLatin1String("stamp_detection")) {
        *mode = DetectionMode::Stamp;
    } else if (normalized == QLatin1String("word_detection")) {
        *mode = DetectionMode::Word;
    } else if (normalized == QLatin1String("ocr_detection")) {
        *mode = DetectionMode::Ocr;
    } else if (normalized == QLatin1String("tissue_detection")) {
        *mode = DetectionMode::Tissue;
    } else if (normalized == QLatin1String("barcode_word_detection")) {
        *mode = DetectionMode::BarcodeWord;
    } else {
        return false;
    }
    return true;
}

ProductRecipe createProductRecipe(const QString &displayName,
                                  DetectionMode detectionMode)
{
    ProductRecipe recipe;
    recipe.recipeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    recipe.displayName = displayName.trimmed();
    recipe.detectionMode = detectionMode;
    return recipe;
}

bool validateProductRecipe(const ProductRecipe &recipe,
                           QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    if (recipe.schemaVersion != ProductRecipe::CurrentSchemaVersion) {
        setError(errorMessage,
                 QStringLiteral("Unsupported recipe schemaVersion: %1")
                 .arg(recipe.schemaVersion));
        return false;
    }
    if (!isCanonicalRecipeId(recipe.recipeId)) {
        setError(errorMessage, QStringLiteral("recipeId must be a canonical UUID."));
        return false;
    }
    if (recipe.displayName.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("displayName must not be empty."));
        return false;
    }
    if (detectionModeId(recipe.detectionMode).isEmpty()) {
        setError(errorMessage, QStringLiteral("detectionMode is invalid."));
        return false;
    }
    if (recipe.detectionMode == DetectionMode::Tissue
            && (!std::isfinite(recipe.tissueParameters.roughnessThreshold)
                || recipe.tissueParameters.roughnessThreshold <= 0.0)) {
        setError(errorMessage,
                 QStringLiteral("Tissue roughnessThreshold must be greater than zero."));
        return false;
    }

    for (auto it = recipe.assets.constBegin(); it != recipe.assets.constEnd(); ++it) {
        if (it.key().trimmed().isEmpty()) {
            setError(errorMessage, QStringLiteral("Recipe asset key must not be empty."));
            return false;
        }
        if (!isValidAssetPath(it.value())) {
            setError(errorMessage,
                     QStringLiteral("Recipe asset must be a relative path under assets/: %1")
                     .arg(it.value()));
            return false;
        }
    }
    return true;
}

QJsonObject productRecipeToJson(const ProductRecipe &recipe)
{
    QJsonObject parameters;
    if (recipe.detectionMode == DetectionMode::Tissue) {
        parameters.insert(QStringLiteral("roughnessThreshold"),
                          recipe.tissueParameters.roughnessThreshold);
    }

    QJsonObject assets;
    for (auto it = recipe.assets.constBegin(); it != recipe.assets.constEnd(); ++it) {
        assets.insert(it.key(), QDir::fromNativeSeparators(it.value()));
    }

    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), recipe.schemaVersion);
    json.insert(QStringLiteral("recipeId"), recipe.recipeId);
    json.insert(QStringLiteral("displayName"), recipe.displayName);
    json.insert(QStringLiteral("detectionMode"), detectionModeId(recipe.detectionMode));
    json.insert(QStringLiteral("parameters"), parameters);
    json.insert(QStringLiteral("assets"), assets);
    return json;
}

bool productRecipeFromJson(const QJsonObject &json,
                           ProductRecipe *recipe,
                           QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!recipe) {
        setError(errorMessage, QStringLiteral("ProductRecipe output is null."));
        return false;
    }

    ProductRecipe candidate;
    const QJsonValue schemaValue = json.value(QStringLiteral("schemaVersion"));
    if (!schemaValue.isDouble()
            || std::floor(schemaValue.toDouble()) != schemaValue.toDouble()) {
        setError(errorMessage,
                 QStringLiteral("Recipe field is missing or is not an integer: schemaVersion"));
        return false;
    }
    candidate.schemaVersion = schemaValue.toInt();

    QString modeId;
    if (!readRequiredString(json, "recipeId", &candidate.recipeId, errorMessage)
            || !readRequiredString(json, "displayName", &candidate.displayName, errorMessage)
            || !readRequiredString(json, "detectionMode", &modeId, errorMessage)) {
        return false;
    }
    if (!detectionModeFromId(modeId, &candidate.detectionMode)) {
        setError(errorMessage,
                 QStringLiteral("Unsupported detectionMode: %1").arg(modeId));
        return false;
    }

    const QJsonValue parametersValue = json.value(QStringLiteral("parameters"));
    if (!parametersValue.isObject()) {
        setError(errorMessage, QStringLiteral("Recipe parameters must be an object."));
        return false;
    }
    const QJsonObject parameters = parametersValue.toObject();
    if (candidate.detectionMode == DetectionMode::Tissue) {
        const QJsonValue thresholdValue =
                parameters.value(QStringLiteral("roughnessThreshold"));
        if (!thresholdValue.isDouble()) {
            setError(errorMessage,
                     QStringLiteral("Tissue roughnessThreshold must be a number."));
            return false;
        }
        candidate.tissueParameters.roughnessThreshold = thresholdValue.toDouble();
    }

    const QJsonValue assetsValue = json.value(QStringLiteral("assets"));
    if (!assetsValue.isObject()) {
        setError(errorMessage, QStringLiteral("Recipe assets must be an object."));
        return false;
    }
    const QJsonObject assets = assetsValue.toObject();
    for (auto it = assets.constBegin(); it != assets.constEnd(); ++it) {
        if (!it.value().isString()) {
            setError(errorMessage,
                     QStringLiteral("Recipe asset value must be a string: %1")
                     .arg(it.key()));
            return false;
        }
        candidate.assets.insert(it.key(), it.value().toString());
    }

    if (!validateProductRecipe(candidate, errorMessage)) {
        return false;
    }
    *recipe = candidate;
    return true;
}

ProductRecipeSnapshot makeProductRecipeSnapshot(const ProductRecipe &recipe,
                                                QString *errorMessage)
{
    if (!validateProductRecipe(recipe, errorMessage)) {
        return ProductRecipeSnapshot();
    }
    return ProductRecipeSnapshot(new ProductRecipe(recipe));
}
