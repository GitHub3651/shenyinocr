#include "template_recipe_publisher.h"

namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

} // namespace

bool publishTemplateRecipe(
        const RecipeStore &store,
        const ProductRecipe &recipeHeader,
        const QVector<TemplateRecipeProfileSource> &profileSources,
        RecipeSelection *publishedSelection,
        QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!publishedSelection) {
        setError(errorMessage,
                 QStringLiteral("Published recipe selection output is null."));
        return false;
    }

    TemplateRecipeAssembly assembly;
    if (!assembleTemplateProductRecipe(recipeHeader,
                                       profileSources,
                                       &assembly,
                                       errorMessage)) {
        return false;
    }
    if (!store.saveRecipe(assembly.recipe,
                          assembly.assetSourcePaths,
                          errorMessage)) {
        return false;
    }

    RecipeSelection candidate;
    if (!loadRecipeSelection(store,
                             assembly.recipe.recipeId,
                             assembly.recipe.detectionMode,
                             &candidate,
                             errorMessage)) {
        return false;
    }

    *publishedSelection = candidate;
    return true;
}
