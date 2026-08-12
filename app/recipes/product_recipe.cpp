#include "product_recipe.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QUuid>

#include <cmath>
#include <limits>

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

bool readRequiredNumber(const QJsonObject &json,
                        const char *key,
                        double *value,
                        QString *errorMessage)
{
    const QJsonValue jsonValue = json.value(QLatin1String(key));
    if (!jsonValue.isDouble() || !std::isfinite(jsonValue.toDouble())) {
        setError(errorMessage,
                 QStringLiteral("Recipe field is missing or is not a finite number: %1")
                 .arg(QLatin1String(key)));
        return false;
    }
    *value = jsonValue.toDouble();
    return true;
}

bool readRequiredInteger(const QJsonObject &json,
                         const char *key,
                         int *value,
                         QString *errorMessage)
{
    double number = 0.0;
    if (!readRequiredNumber(json, key, &number, errorMessage)
            || std::floor(number) != number
            || number < static_cast<double>(std::numeric_limits<int>::min())
            || number > static_cast<double>(std::numeric_limits<int>::max())) {
        if (errorMessage && errorMessage->isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("Recipe field is not an integer: %1")
                     .arg(QLatin1String(key)));
        }
        return false;
    }
    *value = static_cast<int>(number);
    return true;
}

QJsonObject rectFToJson(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

bool rectFFromJson(const QJsonValue &value,
                   QRectF *rect,
                   QString *errorMessage)
{
    if (!value.isObject()) {
        setError(errorMessage, QStringLiteral("Recipe trackingBox must be an object."));
        return false;
    }
    const QJsonObject json = value.toObject();
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    if (!readRequiredNumber(json, "x", &x, errorMessage)
            || !readRequiredNumber(json, "y", &y, errorMessage)
            || !readRequiredNumber(json, "width", &width, errorMessage)
            || !readRequiredNumber(json, "height", &height, errorMessage)) {
        return false;
    }
    *rect = QRectF(x, y, width, height);
    return true;
}

QJsonObject rectToJson(const QRect &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

bool rectFromJson(const QJsonValue &value,
                  QRect *rect,
                  QString *errorMessage)
{
    if (!value.isObject()) {
        setError(errorMessage, QStringLiteral("Recipe character rect must be an object."));
        return false;
    }
    const QJsonObject json = value.toObject();
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (!readRequiredInteger(json, "x", &x, errorMessage)
            || !readRequiredInteger(json, "y", &y, errorMessage)
            || !readRequiredInteger(json, "width", &width, errorMessage)
            || !readRequiredInteger(json, "height", &height, errorMessage)) {
        return false;
    }
    *rect = QRect(x, y, width, height);
    return true;
}

QJsonObject profileToJson(const RecipeProfile &profile,
                          DetectionMode detectionMode)
{
    QJsonObject assetKeys;
    for (auto it = profile.assetKeys.constBegin();
         it != profile.assetKeys.constEnd(); ++it) {
        assetKeys.insert(it.key(), it.value());
    }

    QJsonArray characterBoxes;
    for (const RecipeCharacterBox &box : profile.characterBoxes) {
        QJsonObject boxJson;
        boxJson.insert(QStringLiteral("name"), box.name);
        boxJson.insert(QStringLiteral("rect"), rectToJson(box.rect));
        characterBoxes.append(boxJson);
    }

    QJsonObject sourceSize;
    sourceSize.insert(QStringLiteral("width"),
                      profile.characterSourceImageSize.width());
    sourceSize.insert(QStringLiteral("height"),
                      profile.characterSourceImageSize.height());

    QJsonObject json;
    json.insert(QStringLiteral("name"), profile.name);
    json.insert(QStringLiteral("targetText"), profile.targetText);
    json.insert(QStringLiteral("imageThreshold"), profile.imageThreshold);
    json.insert(QStringLiteral("trackingBox"), rectFToJson(profile.trackingBox));
    json.insert(QStringLiteral("hasValidBoxes"), profile.hasValidBoxes);
    json.insert(QStringLiteral("characterSourceImageSize"), sourceSize);
    json.insert(QStringLiteral("characterBoxes"), characterBoxes);
    json.insert(QStringLiteral("assetKeys"), assetKeys);

    if (detectionMode == DetectionMode::BarcodeWord) {
        QJsonObject barcode;
        barcode.insert(QStringLiteral("formatMask"),
                       static_cast<int>(profile.barcodeParameters.formatMask));
        barcode.insert(QStringLiteral("roiPaddingPercent"),
                       profile.barcodeParameters.roiPaddingPercent);
        barcode.insert(QStringLiteral("maxDecodeTimeMs"),
                       profile.barcodeParameters.maxDecodeTimeMs);
        barcode.insert(QStringLiteral("enableFallback"),
                       profile.barcodeParameters.enableFallback);
        json.insert(QStringLiteral("barcode"), barcode);
    }
    return json;
}

bool profileFromJson(const QJsonValue &value,
                     DetectionMode detectionMode,
                     RecipeProfile *profile,
                     QString *errorMessage)
{
    if (!value.isObject()) {
        setError(errorMessage, QStringLiteral("Recipe profile must be an object."));
        return false;
    }
    const QJsonObject json = value.toObject();
    RecipeProfile candidate;
    if (!readRequiredString(json, "name", &candidate.name, errorMessage)
            || !readRequiredString(json,
                                   "targetText",
                                   &candidate.targetText,
                                   errorMessage)
            || !readRequiredNumber(json,
                                   "imageThreshold",
                                   &candidate.imageThreshold,
                                   errorMessage)
            || !rectFFromJson(json.value(QStringLiteral("trackingBox")),
                              &candidate.trackingBox,
                              errorMessage)) {
        return false;
    }

    const QJsonValue hasValidBoxesValue =
            json.value(QStringLiteral("hasValidBoxes"));
    if (!hasValidBoxesValue.isBool()) {
        setError(errorMessage,
                 QStringLiteral("Recipe profile hasValidBoxes must be a boolean."));
        return false;
    }
    candidate.hasValidBoxes = hasValidBoxesValue.toBool();

    const QJsonValue sourceSizeValue =
            json.value(QStringLiteral("characterSourceImageSize"));
    if (!sourceSizeValue.isObject()) {
        setError(errorMessage,
                 QStringLiteral("Recipe characterSourceImageSize must be an object."));
        return false;
    }
    int sourceWidth = 0;
    int sourceHeight = 0;
    const QJsonObject sourceSize = sourceSizeValue.toObject();
    if (!readRequiredInteger(sourceSize,
                             "width",
                             &sourceWidth,
                             errorMessage)
            || !readRequiredInteger(sourceSize,
                                    "height",
                                    &sourceHeight,
                                    errorMessage)) {
        return false;
    }
    candidate.characterSourceImageSize = QSize(sourceWidth, sourceHeight);

    const QJsonValue characterBoxesValue =
            json.value(QStringLiteral("characterBoxes"));
    if (!characterBoxesValue.isArray()) {
        setError(errorMessage,
                 QStringLiteral("Recipe characterBoxes must be an array."));
        return false;
    }
    const QJsonArray characterBoxes = characterBoxesValue.toArray();
    for (const QJsonValue &boxValue : characterBoxes) {
        if (!boxValue.isObject()) {
            setError(errorMessage,
                     QStringLiteral("Recipe character box must be an object."));
            return false;
        }
        const QJsonObject boxJson = boxValue.toObject();
        RecipeCharacterBox box;
        if (!readRequiredString(boxJson, "name", &box.name, errorMessage)
                || !rectFromJson(boxJson.value(QStringLiteral("rect")),
                                 &box.rect,
                                 errorMessage)) {
            return false;
        }
        candidate.characterBoxes.append(box);
    }

    const QJsonValue assetKeysValue = json.value(QStringLiteral("assetKeys"));
    if (!assetKeysValue.isObject()) {
        setError(errorMessage, QStringLiteral("Recipe profile assetKeys must be an object."));
        return false;
    }
    const QJsonObject assetKeys = assetKeysValue.toObject();
    for (auto it = assetKeys.constBegin(); it != assetKeys.constEnd(); ++it) {
        if (!it.value().isString()) {
            setError(errorMessage,
                     QStringLiteral("Recipe profile asset key must be a string: %1")
                     .arg(it.key()));
            return false;
        }
        candidate.assetKeys.insert(it.key(), it.value().toString());
    }

    if (detectionMode == DetectionMode::BarcodeWord) {
        const QJsonValue barcodeValue = json.value(QStringLiteral("barcode"));
        if (!barcodeValue.isObject()) {
            setError(errorMessage, QStringLiteral("Barcode recipe profile is missing barcode parameters."));
            return false;
        }
        const QJsonObject barcode = barcodeValue.toObject();
        int formatMask = 0;
        if (!readRequiredInteger(barcode,
                                 "formatMask",
                                 &formatMask,
                                 errorMessage)
                || !readRequiredInteger(barcode,
                                        "roiPaddingPercent",
                                        &candidate.barcodeParameters.roiPaddingPercent,
                                        errorMessage)
                || !readRequiredInteger(barcode,
                                        "maxDecodeTimeMs",
                                        &candidate.barcodeParameters.maxDecodeTimeMs,
                                        errorMessage)) {
            return false;
        }
        const QJsonValue enableFallback =
                barcode.value(QStringLiteral("enableFallback"));
        if (!enableFallback.isBool() || formatMask < 0) {
            setError(errorMessage,
                     QStringLiteral("Barcode recipe parameters are invalid."));
            return false;
        }
        candidate.barcodeParameters.formatMask =
                static_cast<unsigned int>(formatMask);
        candidate.barcodeParameters.enableFallback = enableFallback.toBool();
    }

    *profile = candidate;
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

    if (recipe.detectionMode == DetectionMode::Tissue) {
        if (!std::isfinite(recipe.tissueParameters.roughnessThreshold)
                || recipe.tissueParameters.roughnessThreshold <= 0.0) {
            setError(errorMessage,
                     QStringLiteral("Tissue roughnessThreshold must be greater than zero."));
            return false;
        }
        if (!recipe.profiles.isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("Tissue recipes must not contain template profiles."));
            return false;
        }
        return true;
    }

    if (recipe.profiles.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Template-based recipes require at least one profile."));
        return false;
    }

    for (const RecipeProfile &profile : recipe.profiles) {
        if (profile.name.trimmed().isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("Recipe profile name must not be empty."));
            return false;
        }
        if (!std::isfinite(profile.imageThreshold)
                || profile.imageThreshold < 0.0
                || profile.imageThreshold > 100.0) {
            setError(errorMessage,
                     QStringLiteral("Recipe profile imageThreshold must be between 0 and 100."));
            return false;
        }
        if (!profile.hasValidBoxes
                || !std::isfinite(profile.trackingBox.x())
                || !std::isfinite(profile.trackingBox.y())
                || !std::isfinite(profile.trackingBox.width())
                || !std::isfinite(profile.trackingBox.height())
                || profile.trackingBox.width() <= 0.0
                || profile.trackingBox.height() <= 0.0) {
            setError(errorMessage,
                     QStringLiteral("Recipe profile trackingBox is invalid."));
            return false;
        }

        const QSize sourceSize = profile.characterSourceImageSize;
        const bool sourceSizeEmpty = sourceSize.width() == 0
                && sourceSize.height() == 0;
        if ((!sourceSizeEmpty
             && (sourceSize.width() <= 0 || sourceSize.height() <= 0))
                || (sourceSizeEmpty && !profile.characterBoxes.isEmpty())) {
            setError(errorMessage,
                     QStringLiteral("Recipe character source image size is invalid."));
            return false;
        }
        const QRect sourceBounds(QPoint(0, 0), sourceSize);
        for (const RecipeCharacterBox &box : profile.characterBoxes) {
            if (box.name.trimmed().isEmpty()
                    || box.rect.width() <= 0
                    || box.rect.height() <= 0
                    || !sourceBounds.contains(box.rect)) {
                setError(errorMessage,
                         QStringLiteral("Recipe character box is invalid: %1")
                         .arg(box.name));
                return false;
            }
        }

        for (auto it = profile.assetKeys.constBegin();
             it != profile.assetKeys.constEnd(); ++it) {
            if (it.key().trimmed().isEmpty()
                    || it.value().trimmed().isEmpty()
                    || !recipe.assets.contains(it.value())) {
                setError(errorMessage,
                         QStringLiteral("Recipe profile asset reference is invalid: %1")
                         .arg(it.key()));
                return false;
            }
        }

        if (recipe.detectionMode == DetectionMode::BarcodeWord) {
            const BarcodeRecipeParameters &barcode = profile.barcodeParameters;
            if (barcode.formatMask == 0u
                    || (barcode.formatMask & ~3u) != 0u
                    || barcode.roiPaddingPercent < 0
                    || barcode.roiPaddingPercent > 100
                    || barcode.maxDecodeTimeMs <= 0
                    || barcode.maxDecodeTimeMs > 60000) {
                setError(errorMessage,
                         QStringLiteral("Barcode recipe parameters are invalid."));
                return false;
            }
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
    } else {
        QJsonArray profiles;
        for (const RecipeProfile &profile : recipe.profiles) {
            profiles.append(profileToJson(profile, recipe.detectionMode));
        }
        parameters.insert(QStringLiteral("profiles"), profiles);
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
    } else {
        const QJsonValue profilesValue =
                parameters.value(QStringLiteral("profiles"));
        if (!profilesValue.isArray()) {
            setError(errorMessage,
                     QStringLiteral("Template recipe profiles must be an array."));
            return false;
        }
        const QJsonArray profiles = profilesValue.toArray();
        for (const QJsonValue &profileValue : profiles) {
            RecipeProfile profile;
            if (!profileFromJson(profileValue,
                                 candidate.detectionMode,
                                 &profile,
                                 errorMessage)) {
                return false;
            }
            candidate.profiles.append(profile);
        }
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
