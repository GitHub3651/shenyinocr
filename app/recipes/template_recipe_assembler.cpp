#include "template_recipe_assembler.h"

#include <QDir>
#include <QSet>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool validateManifest(const TemplateProfileAssetManifest &manifest,
                      QString *errorMessage)
{
    if (manifest.recipeAssets.size() != manifest.assetSourcePaths.size()
            || manifest.recipeAssets.size()
            != manifest.profileAssetKeys.size()) {
        setError(errorMessage,
                 QStringLiteral("Profile asset manifest maps do not match."));
        return false;
    }
    if (!manifest.profileAssetKeys.contains(
                QStringLiteral("trackingTemplate"))
            || !manifest.profileAssetKeys.contains(
                QStringLiteral("calibration"))) {
        setError(errorMessage,
                 QStringLiteral("Profile asset manifest is missing required assets."));
        return false;
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
    if (recipeHeader.detectionMode != DetectionMode::Word
            && recipeHeader.detectionMode != DetectionMode::BarcodeWord) {
        setError(errorMessage,
                 QStringLiteral("Template recipe assembly only supports the word family."));
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
        if (!validateManifest(manifest, errorMessage)) {
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
