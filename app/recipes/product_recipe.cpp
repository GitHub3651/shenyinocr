#include "recipes/product_recipe.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QUuid>

#include <cmath>
#include <limits>

const int RecipeProfile::DefaultImageThresholdPercent;
const int ProductRecipe::CurrentSchemaVersion;

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message.startsWith(QLatin1String("RECIPE_"))
                ? message
                : QStringLiteral("RECIPE_CONSTRAINT_VIOLATION: %1")
                  .arg(message);
    }
}

bool isCanonicalRecipeId(const QString &recipeId)
{
    const QString value = recipeId.trimmed();
    const QUuid uuid(value);
    return !uuid.isNull()
            && uuid.toString(QUuid::WithoutBraces) == value.toLower();
}

bool hasOnlyKeys(const QJsonObject &object,
                 const QStringList &allowed,
                 const QString &context,
                 QString *errorMessage)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!allowed.contains(it.key())) {
            setError(errorMessage,
                     QStringLiteral("Unsupported %1 field: %2")
                     .arg(context, it.key()));
            return false;
        }
    }
    return true;
}

bool isValidAssetPath(const QString &path)
{
    if (path.trimmed() != path
            || path.contains(QLatin1Char('\\'))
            || path.contains(QLatin1Char(':'))
            || QDir::isAbsolutePath(path)
            || QFileInfo(path).isAbsolute()) {
        return false;
    }
    const QStringList parts = path.split(
                QLatin1Char('/'), Qt::KeepEmptyParts);
    if (parts.size() < 2 || parts.first() != QLatin1String("assets")) {
        return false;
    }
    for (const QString &part : parts) {
        if (part.isEmpty()
                || part == QLatin1String(".")
                || part == QLatin1String("..")) {
            return false;
        }
    }
    return QDir::cleanPath(path) == path;
}

bool readString(const QJsonObject &json,
                const char *key,
                QString *value,
                QString *errorMessage)
{
    const QJsonValue field = json.value(QLatin1String(key));
    if (!field.isString()) {
        setError(errorMessage,
                 QStringLiteral("Recipe field is missing or not a string: %1")
                 .arg(QLatin1String(key)));
        return false;
    }
    *value = field.toString();
    return true;
}

bool readNumber(const QJsonObject &json,
                const char *key,
                double *value,
                QString *errorMessage)
{
    const QJsonValue field = json.value(QLatin1String(key));
    if (!field.isDouble() || !std::isfinite(field.toDouble())) {
        setError(errorMessage,
                 QStringLiteral("Recipe field is missing or not a finite number: %1")
                 .arg(QLatin1String(key)));
        return false;
    }
    *value = field.toDouble();
    return true;
}

bool readInt(const QJsonObject &json,
             const char *key,
             int *value,
             QString *errorMessage)
{
    double number = 0.0;
    if (!readNumber(json, key, &number, errorMessage)
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
        setError(errorMessage, QStringLiteral("trackingRoi must be an object."));
        return false;
    }
    const QJsonObject json = value.toObject();
    if (!hasOnlyKeys(json,
                     QStringList() << "x" << "y" << "width" << "height",
                     QStringLiteral("trackingRoi"), errorMessage)) {
        return false;
    }
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    if (!readNumber(json, "x", &x, errorMessage)
            || !readNumber(json, "y", &y, errorMessage)
            || !readNumber(json, "width", &width, errorMessage)
            || !readNumber(json, "height", &height, errorMessage)) {
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
        setError(errorMessage,
                 QStringLiteral("Character rect must be an object."));
        return false;
    }
    const QJsonObject json = value.toObject();
    if (!hasOnlyKeys(json,
                     QStringList() << "x" << "y" << "width" << "height",
                     QStringLiteral("character rect"), errorMessage)) {
        return false;
    }
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (!readInt(json, "x", &x, errorMessage)
            || !readInt(json, "y", &y, errorMessage)
            || !readInt(json, "width", &width, errorMessage)
            || !readInt(json, "height", &height, errorMessage)) {
        return false;
    }
    *rect = QRect(x, y, width, height);
    return true;
}

bool usesCharacters(DetectionMode mode)
{
    return mode == DetectionMode::Stamp
            || mode == DetectionMode::Word
            || mode == DetectionMode::BarcodeWord;
}

QJsonObject profileToJson(const RecipeProfile &profile,
                          DetectionMode mode)
{
    QJsonObject assets;
    for (auto it = profile.assetKeys.constBegin();
         it != profile.assetKeys.constEnd(); ++it) {
        assets.insert(it.key(), it.value());
    }
    QJsonObject json;
    json.insert(QStringLiteral("name"), profile.name);
    json.insert(QStringLiteral("targetText"), profile.targetText);
    json.insert(QStringLiteral("trackingRoi"),
                rectFToJson(profile.trackingRoi));
    json.insert(QStringLiteral("assetKeys"), assets);

    if (usesCharacters(mode)) {
        json.insert(QStringLiteral("imageThresholdPercent"),
                    profile.imageThresholdPercent);
        QJsonObject size;
        size.insert(QStringLiteral("width"),
                    profile.characterSourceSize.width());
        size.insert(QStringLiteral("height"),
                    profile.characterSourceSize.height());
        json.insert(QStringLiteral("characterSourceSize"), size);
        QJsonArray boxes;
        for (const RecipeCharacterBox &box : profile.characterBoxes) {
            QJsonObject item;
            item.insert(QStringLiteral("name"), box.name);
            item.insert(QStringLiteral("rect"), rectToJson(box.rect));
            boxes.append(item);
        }
        json.insert(QStringLiteral("characterBoxes"), boxes);
    }
    if (mode == DetectionMode::BarcodeWord) {
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
                     DetectionMode mode,
                     RecipeProfile *profile,
                     QString *errorMessage)
{
    if (!value.isObject()) {
        setError(errorMessage, QStringLiteral("Recipe profile must be an object."));
        return false;
    }
    const QJsonObject json = value.toObject();
    QStringList allowed;
    allowed << "name" << "targetText" << "trackingRoi" << "assetKeys";
    if (usesCharacters(mode)) {
        allowed << "imageThresholdPercent"
                << "characterSourceSize"
                << "characterBoxes";
    }
    if (mode == DetectionMode::BarcodeWord) {
        allowed << "barcode";
    }
    if (!hasOnlyKeys(json, allowed, QStringLiteral("profile"),
                     errorMessage)) {
        return false;
    }
    RecipeProfile candidate;
    if (!readString(json, "name", &candidate.name, errorMessage)
            || !readString(json, "targetText", &candidate.targetText,
                           errorMessage)
            || !rectFFromJson(json.value(QStringLiteral("trackingRoi")),
                              &candidate.trackingRoi, errorMessage)) {
        return false;
    }
    const QJsonValue assetValue = json.value(QStringLiteral("assetKeys"));
    if (!assetValue.isObject()) {
        setError(errorMessage, QStringLiteral("assetKeys must be an object."));
        return false;
    }
    const QJsonObject assetKeys = assetValue.toObject();
    for (auto it = assetKeys.constBegin();
         it != assetKeys.constEnd(); ++it) {
        if (!it.value().isString()) {
            setError(errorMessage,
                     QStringLiteral("Asset key value must be a string."));
            return false;
        }
        candidate.assetKeys.insert(it.key(), it.value().toString());
    }

    if (usesCharacters(mode)) {
        if (!readInt(json, "imageThresholdPercent",
                     &candidate.imageThresholdPercent, errorMessage)) {
            return false;
        }
        const QJsonValue sizeValue =
                json.value(QStringLiteral("characterSourceSize"));
        if (!sizeValue.isObject()) {
            setError(errorMessage,
                     QStringLiteral("characterSourceSize must be an object."));
            return false;
        }
        const QJsonObject size = sizeValue.toObject();
        if (!hasOnlyKeys(size, QStringList() << "width" << "height",
                         QStringLiteral("characterSourceSize"), errorMessage)) {
            return false;
        }
        int width = 0;
        int height = 0;
        if (!readInt(size, "width", &width, errorMessage)
                || !readInt(size, "height", &height, errorMessage)) {
            return false;
        }
        candidate.characterSourceSize = QSize(width, height);
        const QJsonValue boxesValue =
                json.value(QStringLiteral("characterBoxes"));
        if (!boxesValue.isArray()) {
            setError(errorMessage, QStringLiteral("characterBoxes must be an array."));
            return false;
        }
        for (const QJsonValue &boxValue : boxesValue.toArray()) {
            if (!boxValue.isObject()) {
                setError(errorMessage, QStringLiteral("Character box must be an object."));
                return false;
            }
            const QJsonObject item = boxValue.toObject();
            if (!hasOnlyKeys(item, QStringList() << "name" << "rect",
                             QStringLiteral("character box"), errorMessage)) {
                return false;
            }
            RecipeCharacterBox box;
            if (!readString(item, "name", &box.name, errorMessage)
                    || !rectFromJson(item.value(QStringLiteral("rect")),
                                     &box.rect, errorMessage)) {
                return false;
            }
            candidate.characterBoxes.append(box);
        }
    }
    if (mode == DetectionMode::BarcodeWord) {
        const QJsonValue barcodeValue = json.value(QStringLiteral("barcode"));
        if (!barcodeValue.isObject()) {
            setError(errorMessage, QStringLiteral("Barcode parameters are missing."));
            return false;
        }
        const QJsonObject barcode = barcodeValue.toObject();
        if (!hasOnlyKeys(barcode,
                         QStringList() << "formatMask"
                         << "roiPaddingPercent" << "maxDecodeTimeMs"
                         << "enableFallback",
                         QStringLiteral("barcode"), errorMessage)) {
            return false;
        }
        int mask = 0;
        if (!readInt(barcode, "formatMask", &mask, errorMessage)
                || !readInt(barcode, "roiPaddingPercent",
                            &candidate.barcodeParameters.roiPaddingPercent,
                            errorMessage)
                || !readInt(barcode, "maxDecodeTimeMs",
                            &candidate.barcodeParameters.maxDecodeTimeMs,
                            errorMessage)) {
            return false;
        }
        const QJsonValue fallback =
                barcode.value(QStringLiteral("enableFallback"));
        if (!fallback.isBool() || mask < 0) {
            setError(errorMessage, QStringLiteral("Barcode parameters are invalid."));
            return false;
        }
        candidate.barcodeParameters.formatMask =
                static_cast<unsigned int>(mask);
        candidate.barcodeParameters.enableFallback = fallback.toBool();
    }
    *profile = candidate;
    return true;
}

bool validateProfile(const ProductRecipe &recipe,
                     const RecipeProfile &profile,
                     QString *errorMessage)
{
    if (profile.name.trimmed().isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Profile name is required."));
        return false;
    }
    const QRectF roi = profile.trackingRoi;
    if (!std::isfinite(roi.x()) || !std::isfinite(roi.y())
            || !std::isfinite(roi.width()) || !std::isfinite(roi.height())
            || roi.x() < 0.0 || roi.y() < 0.0
            || roi.width() <= 0.0 || roi.height() <= 0.0) {
        setError(errorMessage, QStringLiteral("Profile trackingRoi is invalid."));
        return false;
    }
    for (auto it = profile.assetKeys.constBegin();
         it != profile.assetKeys.constEnd(); ++it) {
        const bool commonRole =
                it.key() == QLatin1String("trackingTemplate")
                || it.key() == QLatin1String("calibration")
                || it.key() == QLatin1String("rawImage");
        const bool stampRole =
                recipe.detectionMode == DetectionMode::Stamp
                && it.key() == QLatin1String("stampRing");
        const bool characterRole = usesCharacters(recipe.detectionMode)
                && it.key().startsWith(QLatin1String("character/"));
        if (!commonRole && !stampRole && !characterRole) {
            setError(errorMessage,
                     QStringLiteral("Profile contains a forbidden asset role: %1")
                     .arg(it.key()));
            return false;
        }
    }
    for (const QString &role : requiredTemplateProfileAssetRoles(
             recipe.detectionMode)) {
        if (!profile.assetKeys.contains(role)) {
            setError(errorMessage,
                     QStringLiteral("Profile is missing asset role: %1").arg(role));
            return false;
        }
    }
    for (auto it = profile.assetKeys.constBegin();
         it != profile.assetKeys.constEnd(); ++it) {
        if (it.key().trimmed().isEmpty()
                || it.value().trimmed().isEmpty()
                || !recipe.assets.contains(it.value())) {
            setError(errorMessage,
                     QStringLiteral("Profile asset reference is invalid: %1")
                     .arg(it.key()));
            return false;
        }
    }
    if (!usesCharacters(recipe.detectionMode)) {
        if (!profile.characterBoxes.isEmpty()
                || profile.characterSourceSize != QSize(0, 0)) {
            setError(errorMessage,
                     QStringLiteral("OCR profile contains forbidden character fields."));
            return false;
        }
        return true;
    }
    if (profile.imageThresholdPercent < 0
            || profile.imageThresholdPercent > 100) {
        setError(errorMessage,
                 QStringLiteral("Character profile threshold is invalid."));
        return false;
    }
    bool characterAsset = false;
    for (auto it = profile.assetKeys.constBegin();
         it != profile.assetKeys.constEnd(); ++it) {
        characterAsset = characterAsset
                || it.key().startsWith(QLatin1String("character/"));
    }
    const bool noCharacterData = !characterAsset
            && profile.characterSourceSize == QSize(0, 0)
            && profile.characterBoxes.isEmpty();
    if (!noCharacterData) {
        if (!characterAsset
                || profile.characterSourceSize.width() <= 0
                || profile.characterSourceSize.height() <= 0
                || profile.characterBoxes.isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("Character profile data is incomplete."));
            return false;
        }
        const QRect bounds(QPoint(0, 0), profile.characterSourceSize);
        for (const RecipeCharacterBox &box : profile.characterBoxes) {
            if (box.name.trimmed().isEmpty()
                    || box.rect.width() <= 0 || box.rect.height() <= 0
                    || !bounds.contains(box.rect)) {
                setError(errorMessage,
                         QStringLiteral("Character box is invalid."));
                return false;
            }
        }
    }
    if (recipe.detectionMode == DetectionMode::BarcodeWord) {
        const BarcodeRecipeParameters &barcode = profile.barcodeParameters;
        if (barcode.formatMask == 0u || (barcode.formatMask & ~3u) != 0u
                || barcode.roiPaddingPercent < 0
                || barcode.roiPaddingPercent > 100
                || barcode.maxDecodeTimeMs <= 0
                || barcode.maxDecodeTimeMs > 60000) {
            setError(errorMessage, QStringLiteral("Barcode parameters are invalid."));
            return false;
        }
    }
    return true;
}

} // namespace

bool isTemplateRecipeMode(DetectionMode mode)
{
    return mode != DetectionMode::Tissue;
}

QStringList requiredTemplateProfileAssetRoles(DetectionMode mode)
{
    if (!isTemplateRecipeMode(mode)) return QStringList();
    QStringList roles;
    roles << QStringLiteral("trackingTemplate")
          << QStringLiteral("calibration")
          << QStringLiteral("rawImage");
    if (mode == DetectionMode::Stamp) roles << QStringLiteral("stampRing");
    return roles;
}

ProductRecipe createProductRecipe(const QString &displayName,
                                  DetectionMode mode)
{
    ProductRecipe recipe;
    recipe.recipeId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    recipe.displayName = displayName.trimmed();
    recipe.detectionMode = mode;
    return recipe;
}

bool validateProductRecipe(const ProductRecipe &recipe,
                           QString *errorMessage)
{
    if (errorMessage) errorMessage->clear();
    if (recipe.schemaVersion != ProductRecipe::CurrentSchemaVersion) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_SCHEMA_UNSUPPORTED: schemaVersion=%1")
                 .arg(recipe.schemaVersion));
        return false;
    }
    if (!isCanonicalRecipeId(recipe.recipeId)
            || recipe.displayName.trimmed().isEmpty()
            || detectionModeId(recipe.detectionMode).isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ID_INVALID: recipe identity is invalid."));
        return false;
    }
    QSet<QString> destinations;
    for (auto it = recipe.assets.constBegin();
         it != recipe.assets.constEnd(); ++it) {
        if (it.key().trimmed().isEmpty() || !isValidAssetPath(it.value())
                || destinations.contains(it.value().toLower())) {
            setError(errorMessage, QStringLiteral("Recipe asset is invalid."));
            return false;
        }
        destinations.insert(it.value().toLower());
    }
    if (recipe.detectionMode == DetectionMode::Tissue) {
        if (!std::isfinite(recipe.tissueParameters.roughnessThreshold)
                || recipe.tissueParameters.roughnessThreshold <= 0.0
                || !recipe.profiles.isEmpty() || !recipe.assets.isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_TISSUE_PARAMETERS_INVALID: tissue recipe is invalid."));
            return false;
        }
        return true;
    }
    const bool single = recipe.detectionMode == DetectionMode::Stamp
            || recipe.detectionMode == DetectionMode::Ocr;
    if ((single && recipe.profiles.size() != 1)
            || (!single && recipe.profiles.isEmpty())) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_CONSTRAINT_VIOLATION: recipe profile count is invalid."));
        return false;
    }
    QSet<QString> names;
    QSet<QString> referencedAssetKeys;
    for (const RecipeProfile &profile : recipe.profiles) {
        const QString name = profile.name.trimmed().toLower();
        QString profileError;
        if (names.contains(name)
                || !validateProfile(recipe, profile, &profileError)) {
            if (names.contains(name)) {
                setError(errorMessage,
                         QStringLiteral("RECIPE_CONSTRAINT_VIOLATION: profile names are duplicated."));
            } else {
                QString code;
                switch (recipe.detectionMode) {
                case DetectionMode::Stamp:
                    code = QStringLiteral("RECIPE_STAMP_PROFILE_INVALID");
                    break;
                case DetectionMode::Word:
                    code = QStringLiteral("RECIPE_WORD_PROFILE_INVALID");
                    break;
                case DetectionMode::Ocr:
                    code = QStringLiteral("RECIPE_OCR_PROFILE_INVALID");
                    break;
                case DetectionMode::BarcodeWord:
                    code = QStringLiteral("RECIPE_BARCODE_PROFILE_INVALID");
                    break;
                case DetectionMode::Tissue:
                    break;
                }
                setError(errorMessage,
                         QStringLiteral("%1: %2")
                         .arg(code, profileError));
            }
            return false;
        }
        names.insert(name);
        for (auto it = profile.assetKeys.constBegin();
             it != profile.assetKeys.constEnd(); ++it) {
            if (referencedAssetKeys.contains(it.value())) {
                setError(errorMessage,
                         QStringLiteral("RECIPE_CONSTRAINT_VIOLATION: one asset cannot serve multiple profile roles."));
                return false;
            }
            referencedAssetKeys.insert(it.value());
        }
    }
    if (referencedAssetKeys.size() != recipe.assets.size()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_CONSTRAINT_VIOLATION: recipe contains unreferenced assets."));
        return false;
    }
    return true;
}

QJsonObject productRecipeToJson(const ProductRecipe &recipe)
{
    QJsonObject parameters;
    if (recipe.detectionMode == DetectionMode::Tissue) {
        parameters.insert(QStringLiteral("roughnessThreshold"),
                          recipe.tissueParameters.roughnessThreshold);
    } else if (recipe.detectionMode == DetectionMode::Stamp
               || recipe.detectionMode == DetectionMode::Ocr) {
        parameters.insert(QStringLiteral("profile"),
                          profileToJson(recipe.profiles.first(),
                                        recipe.detectionMode));
    } else {
        QJsonArray profiles;
        for (const RecipeProfile &profile : recipe.profiles) {
            profiles.append(profileToJson(profile, recipe.detectionMode));
        }
        parameters.insert(QStringLiteral("profiles"), profiles);
    }
    QJsonObject assets;
    for (auto it = recipe.assets.constBegin();
         it != recipe.assets.constEnd(); ++it) {
        assets.insert(it.key(), it.value());
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
    if (errorMessage) errorMessage->clear();
    if (!recipe) {
        setError(errorMessage, QStringLiteral("ProductRecipe output is null."));
        return false;
    }
    if (!hasOnlyKeys(json,
                     QStringList() << "schemaVersion" << "recipeId"
                     << "displayName" << "detectionMode"
                     << "parameters" << "assets",
                     QStringLiteral("recipe"), errorMessage)) {
        return false;
    }
    ProductRecipe candidate;
    QString modeId;
    if (!readInt(json, "schemaVersion", &candidate.schemaVersion, errorMessage)
            || !readString(json, "recipeId", &candidate.recipeId, errorMessage)
            || !readString(json, "displayName", &candidate.displayName, errorMessage)
            || !readString(json, "detectionMode", &modeId, errorMessage)
            || !detectionModeFromId(modeId, &candidate.detectionMode)) {
        if (errorMessage && errorMessage->isEmpty()) {
            setError(errorMessage, QStringLiteral("Unsupported detectionMode."));
        }
        return false;
    }
    const QJsonValue parametersValue = json.value(QStringLiteral("parameters"));
    if (!parametersValue.isObject()) {
        setError(errorMessage, QStringLiteral("parameters must be an object."));
        return false;
    }
    const QJsonObject parameters = parametersValue.toObject();
    if (candidate.detectionMode == DetectionMode::Tissue) {
        if (!hasOnlyKeys(parameters, QStringList() << "roughnessThreshold",
                         QStringLiteral("tissue parameters"), errorMessage)
                || !readNumber(parameters, "roughnessThreshold",
                               &candidate.tissueParameters.roughnessThreshold,
                               errorMessage)) return false;
    } else if (candidate.detectionMode == DetectionMode::Stamp
               || candidate.detectionMode == DetectionMode::Ocr) {
        if (!hasOnlyKeys(parameters, QStringList() << "profile",
                         QStringLiteral("single profile parameters"), errorMessage)) {
            return false;
        }
        RecipeProfile profile;
        if (!profileFromJson(parameters.value(QStringLiteral("profile")),
                             candidate.detectionMode, &profile, errorMessage)) {
            return false;
        }
        candidate.profiles.append(profile);
    } else {
        if (!hasOnlyKeys(parameters, QStringList() << "profiles",
                         QStringLiteral("multi profile parameters"), errorMessage)
                || !parameters.value(QStringLiteral("profiles")).isArray()) {
            if (errorMessage && errorMessage->isEmpty()) {
                setError(errorMessage, QStringLiteral("profiles must be an array."));
            }
            return false;
        }
        for (const QJsonValue &value :
             parameters.value(QStringLiteral("profiles")).toArray()) {
            RecipeProfile profile;
            if (!profileFromJson(value, candidate.detectionMode,
                                 &profile, errorMessage)) return false;
            candidate.profiles.append(profile);
        }
    }
    const QJsonValue assetsValue = json.value(QStringLiteral("assets"));
    if (!assetsValue.isObject()) {
        setError(errorMessage, QStringLiteral("assets must be an object."));
        return false;
    }
    const QJsonObject assets = assetsValue.toObject();
    for (auto it = assets.constBegin(); it != assets.constEnd(); ++it) {
        if (!it.value().isString()) {
            setError(errorMessage, QStringLiteral("Asset path must be a string."));
            return false;
        }
        candidate.assets.insert(it.key(), it.value().toString());
    }
    if (!validateProductRecipe(candidate, errorMessage)) return false;
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
