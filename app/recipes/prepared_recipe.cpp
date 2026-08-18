// 文件作用：本文件用于定义配方资源加载完成后的只读运行数据和校验结果。
// 主要职责：定义配方资源加载完成后的只读运行数据和校验结果。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "recipes/prepared_recipe.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

// 函数说明：setError 函数更新或应用对应的配置和状态。
void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message.startsWith(QLatin1String("RECIPE_"))
                || message.startsWith(QLatin1String("PREPARE_"))
                ? message
                : QStringLiteral("RECIPE_CONSTRAINT_VIOLATION: %1")
                  .arg(message);
    }
}

// 函数说明：readImage 函数读取、等待或计算对应的数据。
bool readImage(const QString &path,
               int flags,
               cv::Mat *image,
               QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ASSET_MISSING: %1")
                 .arg(path));
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ASSET_IMAGE_CORRUPT: %1")
                 .arg(path));
        return false;
    }
    const std::vector<uchar> buffer(bytes.begin(), bytes.end());
    cv::Mat decoded;
    try {
        decoded = cv::imdecode(buffer, flags);
    } catch (const cv::Exception &exception) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ASSET_IMAGE_CORRUPT: %1 (%2)")
                 .arg(path, QString::fromStdString(exception.what())));
        return false;
    }
    if (decoded.empty()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ASSET_IMAGE_CORRUPT: %1")
                 .arg(path));
        return false;
    }
    *image = decoded.clone();
    return true;
}

// 函数说明：finitePolygon 函数实现名称所表示的处理步骤。
bool finitePolygon(const std::vector<cv::Point2f> &polygon)
{
    for (const cv::Point2f &point : polygon) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            return false;
        }
    }
    return true;
}

// 函数说明：readCalibration 函数读取、等待或计算对应的数据。
bool readCalibration(const QString &path,
                     std::vector<cv::Point2f> *stamp,
                     std::vector<cv::Point2f> *date,
                     std::vector<cv::Point2f> *barcode,
                     QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_CALIBRATION_INVALID: %1")
                 .arg(path));
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_CALIBRATION_INVALID: %1")
                 .arg(path));
        return false;
    }
    try {
        const std::string yaml(bytes.constData(),
                               static_cast<size_t>(bytes.size()));
        cv::FileStorage storage(
                    yaml,
                    cv::FileStorage::READ | cv::FileStorage::MEMORY);
        if (!storage.isOpened()) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_CALIBRATION_INVALID: %1")
                     .arg(path));
            return false;
        }
        storage["stamp_poly"] >> *stamp;
        storage["date_poly"] >> *date;
        storage["barcode_poly"] >> *barcode;
    } catch (const cv::Exception &exception) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_CALIBRATION_INVALID: %1 (%2)")
                 .arg(path, QString::fromStdString(exception.what())));
        return false;
    }
    if (!finitePolygon(*stamp)
            || !finitePolygon(*date)
            || !finitePolygon(*barcode)) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_CALIBRATION_INVALID: non-finite point"));
        return false;
    }
    return true;
}

// 函数说明：resolvedAssetPath 函数读取、等待或计算对应的数据。
QString resolvedAssetPath(const ProductRecipe &recipe,
                          const RecipeProfile &profile,
                          const QString &role,
                          const QString &recipeDirectoryPath,
                          QString *errorMessage)
{
    const QString key = profile.assetKeys.value(role);
    const QString relative = recipe.assets.value(key);
    if (key.isEmpty() || relative.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ASSET_MISSING: %1").arg(role));
        return QString();
    }
    const QString root = QDir::cleanPath(QDir::fromNativeSeparators(
        QDir(recipeDirectoryPath).absolutePath()));
    const QString path = QDir::cleanPath(QDir::fromNativeSeparators(
        QFileInfo(QDir(root).filePath(relative)).absoluteFilePath()));
    const QString prefix = root.endsWith(QLatin1Char('/'))
            ? root : root + QLatin1Char('/');
    if (!path.startsWith(prefix, Qt::CaseInsensitive)) {
        setError(errorMessage,
                 QStringLiteral("RECIPE_ASSET_PATH_INVALID: %1")
                 .arg(relative));
        return QString();
    }
    return path;
}

// 组件说明：CharacterAsset 数据结构集中保存该流程需要的一组相关数据。
struct CharacterAsset
{
    QString fileName;
    QString normalizedBaseName;
    QString path;
    cv::Mat image;
};

// 函数说明：characterAssetLess 函数实现名称所表示的处理步骤。
bool characterAssetLess(const CharacterAsset &left,
                        const CharacterAsset &right)
{
    const int folded = QString::compare(left.fileName,
                                        right.fileName,
                                        Qt::CaseInsensitive);
    return folded != 0 ? folded < 0 : left.fileName < right.fileName;
}

// 函数说明：matchesTarget 函数执行对应事件或业务处理。
bool matchesTarget(const QString &baseName, const QString &target)
{
    if (baseName == target) {
        return true;
    }
    if (!baseName.startsWith(target)) {
        return false;
    }
    const QString suffix = baseName.mid(target.size());
    return suffix.startsWith(QLatin1Char('_'))
            || suffix.startsWith(QLatin1Char('-'))
            || suffix.startsWith(QLatin1Char('('));
}

// 函数说明：prepareCharacters 函数创建、准备或启动对应流程。
bool prepareCharacters(const ProductRecipe &recipe,
                       const RecipeProfile &profile,
                       const QString &recipeDirectoryPath,
                       std::vector<PreparedRecipeCharacterAsset> *allAssets,
                       std::vector<cv::Mat> *templates,
                       std::vector<int> *indexes,
                       QString *errorMessage)
{
    QVector<CharacterAsset> assets;
    for (auto it = profile.assetKeys.constBegin();
         it != profile.assetKeys.constEnd(); ++it) {
        if (!it.key().startsWith(QLatin1String("character/"))) {
            continue;
        }
        const QString fileName = it.key().mid(10);
        if (fileName.isEmpty() || QFileInfo(fileName).fileName() != fileName) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_CHARACTER_TEMPLATE_INVALID: %1")
                     .arg(it.key()));
            return false;
        }
        CharacterAsset asset;
        asset.fileName = fileName;
        asset.normalizedBaseName = QFileInfo(fileName)
                .completeBaseName().trimmed().toLower();
        asset.path = resolvedAssetPath(recipe, profile, it.key(),
                                       recipeDirectoryPath, errorMessage);
        if (asset.normalizedBaseName.isEmpty() || asset.path.isEmpty()) {
            return false;
        }
        if (!readImage(asset.path, cv::IMREAD_GRAYSCALE,
                       &asset.image, errorMessage)) {
            return false;
        }
        assets.append(asset);
    }
    std::sort(assets.begin(), assets.end(), characterAssetLess);
    allAssets->reserve(static_cast<std::size_t>(assets.size()));
    for (const CharacterAsset &asset : assets) {
        PreparedRecipeCharacterAsset preparedAsset;
        preparedAsset.fileName = asset.fileName;
        preparedAsset.normalizedBaseName = asset.normalizedBaseName;
        preparedAsset.image = asset.image;
        allAssets->push_back(preparedAsset);
    }
    const QStringList targets = preparedRecipeTargetUnits(profile.targetText);
    if (targets.isEmpty()) {
        return true;
    }
    for (int targetIndex = 0; targetIndex < targets.size(); ++targetIndex) {
        for (const CharacterAsset &asset : assets) {
            if (!matchesTarget(asset.normalizedBaseName,
                               targets.at(targetIndex))) {
                continue;
            }
            templates->push_back(asset.image);
            indexes->push_back(targetIndex);
        }
    }
    return true;
}

// 函数说明：prepareProfile 函数创建、准备或启动对应流程。
bool prepareProfile(const ProductRecipe &recipe,
                    const RecipeProfile &profile,
                    const QString &directory,
                    PreparedRecipeProfile *prepared,
                    QString *errorMessage)
{
    PreparedRecipeProfile candidate;
    candidate.definition = profile;
    const QString rawPath = resolvedAssetPath(
                recipe, profile, QStringLiteral("rawImage"),
                directory, errorMessage);
    const QString trackingPath = resolvedAssetPath(
                recipe, profile, QStringLiteral("trackingTemplate"),
                directory, errorMessage);
    const QString calibrationPath = resolvedAssetPath(
                recipe, profile, QStringLiteral("calibration"),
                directory, errorMessage);
    if (rawPath.isEmpty() || trackingPath.isEmpty()
            || calibrationPath.isEmpty()
            || !readImage(rawPath, cv::IMREAD_COLOR,
                          &candidate.rawImage, errorMessage)
            || !readImage(trackingPath, cv::IMREAD_COLOR,
                          &candidate.trackingTemplate, errorMessage)
            || !readCalibration(calibrationPath,
                                &candidate.stampPolygon,
                                &candidate.datePolygon,
                                &candidate.barcodePolygon,
                                errorMessage)) {
        return false;
    }
    const QRectF roi = profile.trackingRoi;
    if (roi.right() > candidate.rawImage.cols
            || roi.bottom() > candidate.rawImage.rows
            || candidate.trackingTemplate.cols
               != static_cast<int>(roi.width())
            || candidate.trackingTemplate.rows
               != static_cast<int>(roi.height())
            || candidate.datePolygon.size() < 3u) {
        setError(errorMessage,
                 QStringLiteral("PREPARE_TRACKING_TEMPLATE_FAILED"));
        return false;
    }
    if (recipe.detectionMode == DetectionMode::BarcodeWord
            && candidate.barcodePolygon.size() != 4u) {
        setError(errorMessage,
                 QStringLiteral("PREPARE_BARCODE_ASSETS_FAILED"));
        return false;
    }
    bool hasCharacterData = profile.characterSourceSize != QSize(0, 0)
            || !profile.characterBoxes.isEmpty();
    for (auto it = profile.assetKeys.constBegin();
         it != profile.assetKeys.constEnd(); ++it) {
        hasCharacterData = hasCharacterData
                || it.key().startsWith(QLatin1String("character/"));
    }
    if ((recipe.detectionMode == DetectionMode::Stamp
         || recipe.detectionMode == DetectionMode::Word
         || recipe.detectionMode == DetectionMode::BarcodeWord)
            && hasCharacterData) {
        const cv::Point2f center(
                    static_cast<float>(profile.trackingRoi.center().x()),
                    static_cast<float>(profile.trackingRoi.center().y()));
        std::vector<cv::Point> absoluteDatePolygon;
        absoluteDatePolygon.reserve(candidate.datePolygon.size());
        for (const cv::Point2f &point : candidate.datePolygon) {
            absoluteDatePolygon.emplace_back(
                        cvRound(center.x + point.x),
                        cvRound(center.y + point.y));
        }
        const cv::Rect characterSourceBounds =
                cv::boundingRect(absoluteDatePolygon)
                & cv::Rect(0, 0,
                           candidate.rawImage.cols,
                           candidate.rawImage.rows);
        if (characterSourceBounds.width <= 0
                || characterSourceBounds.height <= 0
                || profile.characterSourceSize
                   != QSize(characterSourceBounds.width,
                            characterSourceBounds.height)) {
            setError(errorMessage,
                     QStringLiteral("PREPARE_CHARACTER_ASSETS_FAILED: character source size does not match date ROI."));
            return false;
        }
    }
    if (recipe.detectionMode == DetectionMode::Stamp) {
        const QString ringPath = resolvedAssetPath(
                    recipe, profile, QStringLiteral("stampRing"),
                    directory, errorMessage);
        if (ringPath.isEmpty()
                || !readImage(ringPath, cv::IMREAD_GRAYSCALE,
                              &candidate.stampRingTemplate, errorMessage)
                || candidate.stampRingTemplate.cols < 10
                || candidate.stampRingTemplate.rows < 10
                || candidate.stampPolygon.size() < 3u) {
            setError(errorMessage,
                     QStringLiteral("PREPARE_STAMP_ASSETS_FAILED"));
            return false;
        }
    }
    if (recipe.detectionMode == DetectionMode::Stamp
            || recipe.detectionMode == DetectionMode::Word
            || recipe.detectionMode == DetectionMode::BarcodeWord) {
        if (!prepareCharacters(recipe, profile, directory,
                               &candidate.characterAssets,
                               &candidate.characterTemplates,
                               &candidate.characterTemplateTargetIndexes,
                               errorMessage)) {
            return false;
        }
    }
    *prepared = candidate;
    return true;
}

} // namespace

// 函数说明：preparedRecipeTargetUnits 函数创建、准备或启动对应流程。
QStringList preparedRecipeTargetUnits(const QString &targetText)
{
    QStringList units;
    const QRegularExpression expression(
                R"((\d\(\d+\))|(\d)|([A-Za-z])|([\x{4e00}-\x{9fa5}]))");
    QRegularExpressionMatchIterator matches =
            expression.globalMatch(targetText);
    while (matches.hasNext()) {
        const QString value = matches.next().captured(0).toLower();
        if (!value.isEmpty()) {
            units.append(value);
        }
    }
    return units;
}

// 函数说明：preparedRecipeCharacterAssetMatchesTarget 函数创建、准备或启动对应流程。
bool preparedRecipeCharacterAssetMatchesTarget(
        const QString &normalizedBaseName,
        const QString &target)
{
    return matchesTarget(normalizedBaseName.trimmed().toLower(),
                         target.trimmed().toLower());
}

// 函数说明：prepareRecipe 函数创建、准备或启动对应流程。
bool prepareRecipe(const ProductRecipe &recipe,
                   const QString &recipeDirectoryPath,
                   PreparedRecipeSnapshot *preparedRecipe,
                   QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!preparedRecipe) {
        setError(errorMessage,
                 QStringLiteral("PreparedRecipe output is null."));
        return false;
    }
    QString recipeError;
    const ProductRecipeSnapshot snapshot =
            makeProductRecipeSnapshot(recipe, &recipeError);
    if (!snapshot) {
        setError(errorMessage, recipeError);
        return false;
    }
    PreparedRecipe candidate;
    candidate.recipe = snapshot;
    candidate.tissue = recipe.tissueParameters;
    if (recipe.detectionMode != DetectionMode::Tissue) {
        const QFileInfo directoryInfo(recipeDirectoryPath);
        if (!directoryInfo.exists() || directoryInfo.isSymLink()
                || !directoryInfo.isDir()) {
            setError(errorMessage,
                     QStringLiteral("RECIPE_DIRECTORY_MISMATCH"));
            return false;
        }
        for (const RecipeProfile &profile : recipe.profiles) {
            PreparedRecipeProfile preparedProfile;
            if (!prepareProfile(recipe, profile, recipeDirectoryPath,
                                &preparedProfile, errorMessage)) {
                return false;
            }
            candidate.profiles.append(preparedProfile);
        }
    }
    *preparedRecipe = PreparedRecipeSnapshot(new PreparedRecipe(candidate));
    return true;
}
