#ifndef TEMPLATE_PROFILE_ASSETS_H
#define TEMPLATE_PROFILE_ASSETS_H

#include <QMap>
#include <QString>

struct TemplateProfileAssetManifest
{
    QMap<QString, QString> recipeAssets;
    QMap<QString, QString> assetSourcePaths;
    QMap<QString, QString> profileAssetKeys;
};

TemplateProfileAssetManifest buildTemplateProfileAssetManifest(
        const QString &templateDirectoryPath,
        int profileIndex);

#endif // TEMPLATE_PROFILE_ASSETS_H
