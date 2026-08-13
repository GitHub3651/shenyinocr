#ifndef RECIPES_TEMPLATE_CHARACTER_ASSET_WORKSPACE_H
#define RECIPES_TEMPLATE_CHARACTER_ASSET_WORKSPACE_H

#include "template_profile_assets.h"

#include <QMap>
#include <QString>

#include <memory>

class QTemporaryDir;

class TemplateCharacterAssetWorkspace
{
public:
    TemplateCharacterAssetWorkspace();
    ~TemplateCharacterAssetWorkspace();

    bool prepare(const QMap<QString, QString> &assetPathsByRole,
                 QString *errorMessage = nullptr);

    bool isValid() const;
    QString directoryPath() const;
    TemplateProfileAssetManifest assetManifest(int profileIndex) const;

private:
    Q_DISABLE_COPY(TemplateCharacterAssetWorkspace)

    std::unique_ptr<QTemporaryDir> m_directory;
};

#endif // RECIPES_TEMPLATE_CHARACTER_ASSET_WORKSPACE_H
