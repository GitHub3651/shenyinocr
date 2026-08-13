#include "template_recipe_assembler.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool validateManifest(const TemplateProfileAssetManifest &manifest,
                      DetectionMode detectionMode,
                      QString *errorMessage)
{
    if (manifest.recipeAssets.size() != manifest.assetSourcePaths.size()
            || manifest.recipeAssets.size()
            != manifest.profileAssetKeys.size()) {
        setError(errorMessage,
                 QStringLiteral("Profile asset manifest maps do not match."));
        return false;
    }
    const QStringList requiredRoles =
            requiredTemplateProfileAssetRoles(detectionMode);
    for (const QString &requiredRole : requiredRoles) {
        if (!manifest.profileAssetKeys.contains(requiredRole)) {
            setError(errorMessage,
                     QStringLiteral("Profile asset manifest is missing required assets: %1.")
                     .arg(requiredRole));
            return false;
        }
    }

    QSet<QString> referencedAssetKeys;
    for (auto it = manifest.profileAssetKeys.constBegin();
         it != manifest.profileAssetKeys.constEnd();
         ++it) {
        const QString role = it.key().trimmed();
        const QString assetKey = it.value().trimmed();
        if (role.isEmpty()
                || assetKey.isEmpty()
                || !manifest.recipeAssets.contains(assetKey)
                || referencedAssetKeys.contains(assetKey)) {
            setError(errorMessage,
                     QStringLiteral("Profile asset reference is invalid: %1")
                     .arg(it.key()));
            return false;
        }
        referencedAssetKeys.insert(assetKey);
    }

    for (auto it = manifest.recipeAssets.constBegin();
         it != manifest.recipeAssets.constEnd();
         ++it) {
        if (it.key().trimmed().isEmpty()
                || it.value().trimmed().isEmpty()
                || !manifest.assetSourcePaths.contains(it.key())
                || manifest.assetSourcePaths.value(it.key()).trimmed().isEmpty()
                || !referencedAssetKeys.contains(it.key())) {
            setError(errorMessage,
                     QStringLiteral("Profile asset entry is incomplete: %1")
                     .arg(it.key()));
            return false;
        }
    }
    return true;
}

bool resolveSelectedAssetSource(const RecipeSelection &selection,
                                const QString &profileName,
                                const QString &role,
                                const QString &assetKey,
                                const QString &resolvedPath,
                                QString *absoluteSourcePath,
                                QString *errorMessage)
{
    if (!selection.recipe->assets.contains(assetKey)) {
        setError(errorMessage,
                 QStringLiteral("Selected recipe profile %1 has an invalid asset key for role %2.")
                 .arg(profileName, role));
        return false;
    }

    const QFileInfo sourceInfo(resolvedPath);
    if (!sourceInfo.exists()
            || sourceInfo.isSymLink()
            || !sourceInfo.isFile()
            || sourceInfo.size() <= 0) {
        setError(errorMessage,
                 QStringLiteral("Selected recipe profile %1 has a missing or invalid asset for role %2.")
                 .arg(profileName, role));
        return false;
    }

    const QString expectedPath = QDir(selection.recipeDirectoryPath).filePath(
                QDir::cleanPath(QDir::fromNativeSeparators(
                                    selection.recipe->assets.value(assetKey))));
    const QString canonicalSourcePath = sourceInfo.canonicalFilePath();
    const QString canonicalExpectedPath = QFileInfo(expectedPath)
            .canonicalFilePath();
    if (canonicalSourcePath.isEmpty()
            || canonicalExpectedPath.isEmpty()
            || QDir::cleanPath(canonicalSourcePath).compare(
                QDir::cleanPath(canonicalExpectedPath),
                Qt::CaseInsensitive) != 0) {
        setError(errorMessage,
                 QStringLiteral("Selected recipe profile %1 asset path does not match role %2.")
                 .arg(profileName, role));
        return false;
    }

    *absoluteSourcePath = sourceInfo.absoluteFilePath();
    return true;
}

} // namespace

bool assembleTemplateProductRecipe(
        const ProductRecipe &recipeHeader,
        const QVector<TemplateRecipeProfileSource> &profileSources,
        TemplateRecipeAssembly *assembly,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!assembly) {
        setError(errorMessage,
                 QStringLiteral("Template recipe assembly output is null."));
        return false;
    }
    if (!isTemplateRecipeMode(recipeHeader.detectionMode)) {
        setError(errorMessage,
                 QStringLiteral("Template recipe assembly only supports template-based modes."));
        return false;
    }
    if (profileSources.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Template recipe assembly requires at least one profile."));
        return false;
    }

    TemplateRecipeAssembly candidate;
    candidate.recipe = recipeHeader;
    candidate.recipe.profiles.clear();
    candidate.recipe.assets.clear();

    QSet<QString> normalizedAssetKeys;
    QSet<QString> normalizedTargetPaths;
    for (const TemplateRecipeProfileSource &source : profileSources) {
        const TemplateProfileAssetManifest &manifest = source.assetManifest;
        if (!validateManifest(manifest,
                              recipeHeader.detectionMode,
                              errorMessage)) {
            return false;
        }

        RecipeProfile profile = source.profile;
        profile.assetKeys = manifest.profileAssetKeys;

        for (auto it = manifest.recipeAssets.constBegin();
             it != manifest.recipeAssets.constEnd();
             ++it) {
            const QString normalizedAssetKey = it.key().trimmed().toLower();
            const QString normalizedTargetPath = QDir::cleanPath(
                        QDir::fromNativeSeparators(it.value())).toLower();
            if (normalizedAssetKeys.contains(normalizedAssetKey)) {
                setError(errorMessage,
                         QStringLiteral("Duplicate recipe asset key: %1")
                         .arg(it.key()));
                return false;
            }
            if (normalizedTargetPaths.contains(normalizedTargetPath)) {
                setError(errorMessage,
                         QStringLiteral("Duplicate recipe asset destination: %1")
                         .arg(it.value()));
                return false;
            }

            normalizedAssetKeys.insert(normalizedAssetKey);
            normalizedTargetPaths.insert(normalizedTargetPath);
            candidate.recipe.assets.insert(it.key(), it.value());
            candidate.assetSourcePaths.insert(
                        it.key(), manifest.assetSourcePaths.value(it.key()));
        }
        candidate.recipe.profiles.append(profile);
    }

    if (!validateProductRecipe(candidate.recipe, errorMessage)) {
        return false;
    }

    *assembly = candidate;
    return true;
}

bool assembleSelectedTemplateRecipe(
        const RecipeSelection &selection,
        TemplateRecipeAssembly *assembly,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!assembly) {
        setError(errorMessage,
                 QStringLiteral("Selected template recipe assembly output is null."));
        return false;
    }
    if (!selection.recipe) {
        setError(errorMessage,
                 QStringLiteral("Selected template recipe snapshot is null."));
        return false;
    }
    if (!isTemplateRecipeMode(selection.recipe->detectionMode)) {
        setError(errorMessage,
                 QStringLiteral("Selected template recipe assembly only supports template-based modes."));
        return false;
    }
    if (selection.recipeDirectoryPath.trimmed().isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Selected template recipe directory is empty."));
        return false;
    }
    if (selection.recipe->profiles.isEmpty()
            || selection.profiles.size()
            != selection.recipe->profiles.size()) {
        setError(errorMessage,
                 QStringLiteral("Selected template recipe profile count does not match the snapshot."));
        return false;
    }

    TemplateRecipeAssembly candidate;
    candidate.recipe = *selection.recipe;

    for (int profileIndex = 0;
         profileIndex < candidate.recipe.profiles.size();
         ++profileIndex) {
        const RecipeProfile &recipeProfile =
                candidate.recipe.profiles.at(profileIndex);
        const ResolvedRecipeProfile &resolvedProfile =
                selection.profiles.at(profileIndex);
        const QString profileName = recipeProfile.name;
        if (resolvedProfile.profile.name != recipeProfile.name
                || resolvedProfile.profile.assetKeys
                != recipeProfile.assetKeys) {
            setError(errorMessage,
                     QStringLiteral("Selected recipe profile mapping does not match the snapshot: %1")
                     .arg(profileName));
            return false;
        }

        const QStringList requiredRoles =
                requiredTemplateProfileAssetRoles(
                    candidate.recipe.detectionMode);
        for (const QString &requiredRole : requiredRoles) {
            if (!recipeProfile.assetKeys.contains(requiredRole)
                    || !resolvedProfile.assetPathsByRole.contains(
                        requiredRole)) {
                setError(errorMessage,
                         QStringLiteral("Selected recipe profile %1 is missing required asset role %2.")
                         .arg(profileName, requiredRole));
                return false;
            }
        }

        for (auto it = recipeProfile.assetKeys.constBegin();
             it != recipeProfile.assetKeys.constEnd();
             ++it) {
            const QString role = it.key();
            const QString assetKey = it.value();
            if (!resolvedProfile.assetPathsByRole.contains(role)) {
                setError(errorMessage,
                         QStringLiteral("Selected recipe profile %1 is missing asset role %2.")
                         .arg(profileName, role));
                return false;
            }

            QString sourcePath;
            if (!resolveSelectedAssetSource(
                        selection,
                        profileName,
                        role,
                        assetKey,
                        resolvedProfile.assetPathsByRole.value(role),
                        &sourcePath,
                        errorMessage)) {
                return false;
            }

            if (candidate.assetSourcePaths.contains(assetKey)
                    && QFileInfo(candidate.assetSourcePaths.value(assetKey))
                    .absoluteFilePath().compare(
                        QFileInfo(sourcePath).absoluteFilePath(),
                        Qt::CaseInsensitive) != 0) {
                setError(errorMessage,
                         QStringLiteral("Selected recipe asset source is inconsistent: %1")
                         .arg(assetKey));
                return false;
            }
            candidate.assetSourcePaths.insert(assetKey, sourcePath);
        }

        if (resolvedProfile.assetPathsByRole.size()
                != recipeProfile.assetKeys.size()) {
            setError(errorMessage,
                     QStringLiteral("Selected recipe profile %1 contains an unexpected asset role.")
                     .arg(profileName));
            return false;
        }
    }

    if (candidate.assetSourcePaths.size() != candidate.recipe.assets.size()) {
        setError(errorMessage,
                 QStringLiteral("Selected template recipe has unresolved asset sources."));
        return false;
    }
    if (!validateProductRecipe(candidate.recipe, errorMessage)) {
        return false;
    }

    *assembly = candidate;
    return true;
}

bool replaceTemplateRecipeProfileAssets(
        TemplateRecipeAssembly *assembly,
        int profileIndex,
        const RecipeProfile &profile,
        const TemplateProfileAssetManifest &assetManifest,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!assembly) {
        setError(errorMessage,
                 QStringLiteral("Template recipe asset replacement assembly is null."));
        return false;
    }
    if (profileIndex < 0
            || profileIndex >= assembly->recipe.profiles.size()) {
        setError(errorMessage,
                 QStringLiteral("Template recipe asset replacement profile index is invalid."));
        return false;
    }
    if (profile.name != assembly->recipe.profiles.at(profileIndex).name) {
        setError(errorMessage,
                 QStringLiteral("Template recipe asset replacement cannot change profile identity."));
        return false;
    }

    QVector<TemplateRecipeProfileSource> profileSources;
    profileSources.reserve(assembly->recipe.profiles.size());
    for (int currentIndex = 0;
         currentIndex < assembly->recipe.profiles.size();
         ++currentIndex) {
        TemplateRecipeProfileSource source;
        source.profile = currentIndex == profileIndex
                ? profile
                : assembly->recipe.profiles.at(currentIndex);
        if (currentIndex == profileIndex) {
            source.assetManifest = assetManifest;
        } else {
            source.assetManifest.profileAssetKeys =
                    source.profile.assetKeys;
            for (auto it = source.profile.assetKeys.constBegin();
                 it != source.profile.assetKeys.constEnd();
                 ++it) {
                const QString assetKey = it.value();
                if (assembly->recipe.assets.contains(assetKey)) {
                    source.assetManifest.recipeAssets.insert(
                                assetKey,
                                assembly->recipe.assets.value(assetKey));
                }
                if (assembly->assetSourcePaths.contains(assetKey)) {
                    source.assetManifest.assetSourcePaths.insert(
                                assetKey,
                                assembly->assetSourcePaths.value(assetKey));
                }
            }
        }
        profileSources.append(source);
    }

    TemplateRecipeAssembly candidate;
    if (!assembleTemplateProductRecipe(assembly->recipe,
                                       profileSources,
                                       &candidate,
                                       errorMessage)) {
        return false;
    }

    *assembly = candidate;
    return true;
}
