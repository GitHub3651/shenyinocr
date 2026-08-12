#include "template_profile_assets.h"

#include <QDir>
#include <QFileInfo>

namespace {

void addAsset(TemplateProfileAssetManifest *manifest,
              const QString &assetKey,
              const QString &profileRole,
              const QString &sourcePath,
              const QString &relativeTargetPath)
{
    if (!manifest || !QFileInfo::exists(sourcePath)) {
        return;
    }

    manifest->recipeAssets.insert(assetKey, relativeTargetPath);
    manifest->assetSourcePaths.insert(assetKey,
                                      QFileInfo(sourcePath).absoluteFilePath());
    manifest->profileAssetKeys.insert(profileRole, assetKey);
}

bool isSystemTemplateImage(const QString &fileName)
{
    const QString normalizedName = fileName.trimmed().toLower();
    return normalizedName == QStringLiteral("template_raw.png")
            || normalizedName == QStringLiteral("tracking_template.bmp")
            || normalizedName == QStringLiteral("template_ring.bmp");
}

} // namespace

TemplateProfileAssetManifest buildTemplateProfileAssetManifest(
        const QString &templateDirectoryPath,
        int profileIndex)
{
    TemplateProfileAssetManifest manifest;
    const QDir directory(templateDirectoryPath);
    if (profileIndex < 0 || !directory.exists()) {
        return manifest;
    }

    const QString assetKeyPrefix = QStringLiteral("profile%1").arg(profileIndex);
    const QString targetPathPrefix =
            QStringLiteral("assets/profiles/%1").arg(profileIndex);
    addAsset(&manifest,
             assetKeyPrefix + QStringLiteral(".trackingTemplate"),
             QStringLiteral("trackingTemplate"),
             directory.filePath(QStringLiteral("tracking_template.bmp")),
             targetPathPrefix + QStringLiteral("/tracking_template.bmp"));
    addAsset(&manifest,
             assetKeyPrefix + QStringLiteral(".calibration"),
             QStringLiteral("calibration"),
             directory.filePath(QStringLiteral("calibrate_config.yaml")),
             targetPathPrefix + QStringLiteral("/calibrate_config.yaml"));
    addAsset(&manifest,
             assetKeyPrefix + QStringLiteral(".rawImage"),
             QStringLiteral("rawImage"),
             directory.filePath(QStringLiteral("template_raw.png")),
             targetPathPrefix + QStringLiteral("/template_raw.png"));

    static const QStringList imageFilters = {
        QStringLiteral("*.png"),
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.bmp"),
        QStringLiteral("*.tiff")
    };
    const QFileInfoList imageFiles = directory.entryInfoList(
                imageFilters,
                QDir::Files | QDir::NoDotAndDotDot,
                QDir::Name | QDir::IgnoreCase);
    int characterIndex = 0;
    for (const QFileInfo &fileInfo : imageFiles) {
        if (isSystemTemplateImage(fileInfo.fileName())) {
            continue;
        }

        const QString assetKey =
                assetKeyPrefix
                + QStringLiteral(".character%1")
                  .arg(characterIndex, 4, 10, QLatin1Char('0'));
        const QString role = QStringLiteral("character/")
                + fileInfo.fileName();
        const QString relativePath =
                targetPathPrefix
                + QStringLiteral("/character_templates/")
                + fileInfo.fileName();
        addAsset(&manifest,
                 assetKey,
                 role,
                 fileInfo.absoluteFilePath(),
                 relativePath);
        ++characterIndex;
    }

    return manifest;
}
