#include "recipe_selection.h"

#include <QDir>
#include <QFileInfo>

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool resolveProfileAssets(const ProductRecipe &recipe,
                          const QString &recipeDirectoryPath,
                          const RecipeProfile &profile,
                          ResolvedRecipeProfile *resolvedProfile,
                          QString *errorMessage)
{
    ResolvedRecipeProfile candidate;
    candidate.profile = profile;

    for (auto it = profile.assetKeys.constBegin();
         it != profile.assetKeys.constEnd();
         ++it) {
        const QString role = it.key().trimmed();
        const QString assetKey = it.value().trimmed();
        if (role.isEmpty()
                || assetKey.isEmpty()
                || !recipe.assets.contains(assetKey)) {
            setError(errorMessage,
                     QStringLiteral("Recipe profile asset reference is invalid: %1")
                     .arg(it.key()));
            return false;
        }

        const QString assetPath = QDir(recipeDirectoryPath).filePath(
                    QDir::cleanPath(QDir::fromNativeSeparators(
                                        recipe.assets.value(assetKey))));
        const QFileInfo assetInfo(assetPath);
        if (!assetInfo.exists()
                || assetInfo.isSymLink()
                || !assetInfo.isFile()
                || assetInfo.size() <= 0) {
            setError(errorMessage,
                     QStringLiteral("Selected recipe asset is missing or invalid: %1")
                     .arg(assetKey));
            return false;
        }
        candidate.assetPathsByRole.insert(role,
                                          assetInfo.absoluteFilePath());
    }

    if (!candidate.assetPathsByRole.contains(
                QStringLiteral("trackingTemplate"))
            || !candidate.assetPathsByRole.contains(
                QStringLiteral("calibration"))) {
        setError(errorMessage,
                 QStringLiteral("Selected recipe profile is missing required runtime assets: %1")
                 .arg(profile.name));
        return false;
    }

    *resolvedProfile = candidate;
    return true;
}

} // namespace

bool loadRecipeSelection(const RecipeStore &store,
                         const QString &recipeId,
                         DetectionMode expectedMode,
                         RecipeSelection *selection,
                         QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!selection) {
        setError(errorMessage,
                 QStringLiteral("RecipeSelection output is null."));
        return false;
    }

    ProductRecipe recipe;
    if (!store.loadRecipe(recipeId, &recipe, errorMessage)) {
        return false;
    }
    if (recipe.detectionMode != expectedMode) {
        setError(errorMessage,
                 QStringLiteral("Selected recipe mode does not match the current mode."));
        return false;
    }

    RecipeSelection candidate;
    candidate.recipeDirectoryPath = store.recipeDirectoryPath(recipe.recipeId);
    if (candidate.recipeDirectoryPath.isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Selected recipe directory is invalid."));
        return false;
    }

    if (recipe.detectionMode != DetectionMode::Tissue) {
        for (const RecipeProfile &profile : recipe.profiles) {
            ResolvedRecipeProfile resolvedProfile;
            if (!resolveProfileAssets(recipe,
                                      candidate.recipeDirectoryPath,
                                      profile,
                                      &resolvedProfile,
                                      errorMessage)) {
                return false;
            }
            candidate.profiles.append(resolvedProfile);
        }
    }

    candidate.recipe = makeProductRecipeSnapshot(recipe, errorMessage);
    if (!candidate.recipe) {
        return false;
    }

    *selection = candidate;
    return true;
}
