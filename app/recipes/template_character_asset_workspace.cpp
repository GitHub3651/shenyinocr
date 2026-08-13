#include "template_character_asset_workspace.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool copyAsset(const QString &workspacePath,
               const QString &sourcePath,
               const QString &targetFileName,
               QString *errorMessage)
{
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()
            || sourceInfo.isSymLink()
            || !sourceInfo.isFile()
            || sourceInfo.size() <= 0
            || QFileInfo(targetFileName).fileName() != targetFileName) {
        setError(errorMessage,
                 QStringLiteral("Recipe character edit asset is invalid: %1")
                 .arg(sourcePath));
        return false;
    }

    const QString targetPath = QDir(workspacePath).filePath(targetFileName);
    if (!QFile::copy(sourceInfo.absoluteFilePath(), targetPath)) {
        setError(errorMessage,
                 QStringLiteral("Recipe character edit asset cannot be staged: %1")
                 .arg(targetFileName));
        return false;
    }
    return true;
}

} // namespace

TemplateCharacterAssetWorkspace::TemplateCharacterAssetWorkspace()
    : m_directory(new QTemporaryDir)
{
}

TemplateCharacterAssetWorkspace::~TemplateCharacterAssetWorkspace()
{
}

bool TemplateCharacterAssetWorkspace::prepare(
        const QMap<QString, QString> &assetPathsByRole,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!isValid()) {
        setError(errorMessage,
                 QStringLiteral("Recipe character edit workspace is unavailable."));
        return false;
    }

    const QString workspacePath = directoryPath();
    if (!copyAsset(workspacePath,
                   assetPathsByRole.value(QStringLiteral("rawImage")),
                   QStringLiteral("template_raw.png"),
                   errorMessage)
            || !copyAsset(workspacePath,
                          assetPathsByRole.value(
                              QStringLiteral("trackingTemplate")),
                          QStringLiteral("tracking_template.bmp"),
                          errorMessage)
            || !copyAsset(workspacePath,
                          assetPathsByRole.value(
                              QStringLiteral("calibration")),
                          QStringLiteral("calibrate_config.yaml"),
                          errorMessage)) {
        return false;
    }

    const QString stampRingPath =
            assetPathsByRole.value(QStringLiteral("stampRing")).trimmed();
    if (!stampRingPath.isEmpty()
            && !copyAsset(workspacePath,
                          stampRingPath,
                          QStringLiteral("template_ring.bmp"),
                          errorMessage)) {
        return false;
    }

    const QString characterRolePrefix = QStringLiteral("character/");
    for (auto it = assetPathsByRole.constBegin();
         it != assetPathsByRole.constEnd();
         ++it) {
        if (!it.key().startsWith(characterRolePrefix)) {
            continue;
        }
        const QString fileName = it.key().mid(
                    characterRolePrefix.size()).trimmed();
        if (!copyAsset(workspacePath,
                       it.value(),
                       fileName,
                       errorMessage)) {
            return false;
        }
    }
    return true;
}

bool TemplateCharacterAssetWorkspace::isValid() const
{
    return m_directory && m_directory->isValid();
}

QString TemplateCharacterAssetWorkspace::directoryPath() const
{
    return isValid() ? m_directory->path() : QString();
}

TemplateProfileAssetManifest
TemplateCharacterAssetWorkspace::assetManifest(int profileIndex) const
{
    return buildTemplateProfileAssetManifest(directoryPath(), profileIndex);
}
