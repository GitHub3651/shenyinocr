// 文件作用：本文件用于管理模板图、字符图和校准文件等配方资源的读取与写入。
// 主要职责：管理模板图、字符图和校准文件等配方资源的读取与写入。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "recipes/recipe_asset_service.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <opencv2/imgcodecs.hpp>

namespace {

// 函数说明：setError 函数更新或应用对应的配置和状态。
void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

// 函数说明：writeBytes 函数保存或发布对应的数据和资源。
bool writeBytes(const QString &path,
                const QByteArray &bytes,
                QString *errorMessage)
{
    QSaveFile file(path);
    if (bytes.isEmpty()
            || !file.open(QIODevice::WriteOnly)
            || file.write(bytes) != bytes.size()
            || !file.commit()) {
        setError(errorMessage,
                 file.errorString().isEmpty()
                 ? QStringLiteral("资源文件写入失败。")
                 : file.errorString());
        return false;
    }
    return true;
}

// 函数说明：writeCvImage 函数保存或发布对应的数据和资源。
bool writeCvImage(const QString &path,
                  const cv::Mat &image,
                  const char *extension,
                  QString *errorMessage)
{
    if (image.empty()) {
        setError(errorMessage, QStringLiteral("图像资源为空。"));
        return false;
    }
    std::vector<uchar> encoded;
    try {
        if (!cv::imencode(extension, image, encoded)) {
            setError(errorMessage, QStringLiteral("图像资源编码失败。"));
            return false;
        }
    } catch (const cv::Exception &error) {
        setError(errorMessage,
                 QString::fromLocal8Bit(error.what()));
        return false;
    }
    return writeBytes(
                path,
                QByteArray(reinterpret_cast<const char *>(encoded.data()),
                           static_cast<int>(encoded.size())),
                errorMessage);
}

// 函数说明：writeCalibration 函数保存或发布对应的数据和资源。
bool writeCalibration(const QString &path,
                      const InitialRecipeProfileAssets &assets,
                      QString *errorMessage)
{
    cv::FileStorage storage(
                "calibrate_config.yaml",
                cv::FileStorage::WRITE | cv::FileStorage::MEMORY);
    storage << "stamp_poly" << assets.stampPolygon;
    storage << "date_poly" << assets.datePolygon;
    storage << "barcode_poly" << assets.barcodePolygon;
    const std::string yaml = storage.releaseAndGetString();
    return writeBytes(
                path,
                QByteArray(yaml.data(), static_cast<int>(yaml.size())),
                errorMessage);
}

// 函数说明：writeQImage 函数保存或发布对应的数据和资源。
bool writeQImage(const QString &path,
                 const QImage &image,
                 QString *errorMessage)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (image.isNull()
            || !buffer.open(QIODevice::WriteOnly)
            || !image.save(&buffer, "PNG")) {
        setError(errorMessage,
                 QStringLiteral("字符模板图像编码失败。"));
        return false;
    }
    return writeBytes(path, bytes, errorMessage);
}

// 函数说明：addAsset 函数实现名称所表示的处理步骤。
void addAsset(ProductRecipe *recipe,
              RecipeProfile *profile,
              QMap<QString, QString> *sources,
              const QString &key,
              const QString &role,
              const QString &source,
              const QString &relativePath)
{
    recipe->assets.insert(key, relativePath);
    profile->assetKeys.insert(role, key);
    sources->insert(key, source);
}

} // namespace

// 函数说明：stageInitialProfileAssets 函数实现名称所表示的处理步骤。
bool RecipeAssetService::stageInitialProfileAssets(
        const QString &workspacePath,
        const InitialRecipeProfileAssets &assets,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage) const
{
    if (!recipe || !profile || !assetSourcePaths
            || workspacePath.trimmed().isEmpty()
            || assets.profileIndex < 0
            || assets.rawImage.empty()
            || assets.trackingImageRect.width <= 0
            || assets.trackingImageRect.height <= 0
            || assets.trackingImageRect.x < 0
            || assets.trackingImageRect.y < 0
            || assets.trackingImageRect.x
               + assets.trackingImageRect.width > assets.rawImage.cols
            || assets.trackingImageRect.y
               + assets.trackingImageRect.height > assets.rawImage.rows) {
        setError(errorMessage,
                 QStringLiteral("配方资源输入无效。"));
        return false;
    }

    QDir workspace(workspacePath);
    if (!workspace.exists() && !QDir().mkpath(workspacePath)) {
        setError(errorMessage,
                 QStringLiteral("配方编辑工作区不可用。"));
        return false;
    }
    const QString profilePrefix = QStringLiteral("profile%1_")
            .arg(assets.profileIndex);
    const QString rawPath = workspace.filePath(
                profilePrefix + QStringLiteral("template_raw.png"));
    const QString trackingPath = workspace.filePath(
                profilePrefix + QStringLiteral("tracking_template.bmp"));
    const QString calibrationPath = workspace.filePath(
                profilePrefix + QStringLiteral("calibrate_config.yaml"));
    const cv::Mat trackingTemplate =
            assets.rawImage(assets.trackingImageRect).clone();
    if (!writeCvImage(rawPath, assets.rawImage, ".png", errorMessage)
            || !writeCvImage(trackingPath, trackingTemplate,
                             ".bmp", errorMessage)
            || !writeCalibration(calibrationPath, assets, errorMessage)) {
        return false;
    }

    const QString keyPrefix = QStringLiteral("profile%1.")
            .arg(assets.profileIndex);
    const QString relativePrefix =
            QStringLiteral("assets/profiles/%1/")
            .arg(assets.profileIndex);
    addAsset(recipe, profile, assetSourcePaths,
             keyPrefix + QStringLiteral("rawImage"),
             QStringLiteral("rawImage"), rawPath,
             relativePrefix + QStringLiteral("template_raw.png"));
    addAsset(recipe, profile, assetSourcePaths,
             keyPrefix + QStringLiteral("trackingTemplate"),
             QStringLiteral("trackingTemplate"), trackingPath,
             relativePrefix + QStringLiteral("tracking_template.bmp"));
    addAsset(recipe, profile, assetSourcePaths,
             keyPrefix + QStringLiteral("calibration"),
             QStringLiteral("calibration"), calibrationPath,
             relativePrefix + QStringLiteral("calibrate_config.yaml"));

    if (!assets.stampRing.empty()) {
        const QString ringPath = workspace.filePath(
                    profilePrefix + QStringLiteral("template_ring.bmp"));
        if (!writeCvImage(ringPath, assets.stampRing,
                          ".bmp", errorMessage)) {
            return false;
        }
        addAsset(recipe, profile, assetSourcePaths,
                 keyPrefix + QStringLiteral("stampRing"),
                 QStringLiteral("stampRing"), ringPath,
                 relativePrefix + QStringLiteral("template_ring.bmp"));
    }
    return true;
}

// 函数说明：stageCharacterAssets 函数实现名称所表示的处理步骤。
bool RecipeAssetService::stageCharacterAssets(
        const QString &workspacePath,
        int profileIndex,
        const QMap<QString, QImage> &characterImages,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage) const
{
    if (!recipe || !profile || !assetSourcePaths
            || workspacePath.trimmed().isEmpty()
            || profileIndex < 0 || characterImages.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("字符模板资源输入无效。"));
        return false;
    }

    const QStringList oldRoles = profile->assetKeys.keys();
    for (const QString &role : oldRoles) {
        if (!role.startsWith(QLatin1String("character/"))) {
            continue;
        }
        const QString key = profile->assetKeys.take(role);
        recipe->assets.remove(key);
        assetSourcePaths->remove(key);
    }

    QDir workspace(workspacePath);
    int characterIndex = 0;
    for (auto it = characterImages.constBegin();
         it != characterImages.constEnd(); ++it, ++characterIndex) {
        const QString sourcePath = workspace.filePath(
                    QStringLiteral("profile%1_%2")
                    .arg(profileIndex).arg(it.key()));
        if (!writeQImage(sourcePath, it.value(), errorMessage)) {
            return false;
        }
        const QString key =
                QStringLiteral("profile%1.character%2")
                .arg(profileIndex)
                .arg(characterIndex, 4, 10, QLatin1Char('0'));
        recipe->assets.insert(
                    key,
                    QStringLiteral(
                        "assets/profiles/%1/character_templates/%2")
                    .arg(profileIndex).arg(it.key()));
        profile->assetKeys.insert(
                    QStringLiteral("character/") + it.key(), key);
        assetSourcePaths->insert(key, sourcePath);
    }
    return true;
}
