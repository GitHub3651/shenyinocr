#ifndef TEMPLATE_RECIPE_ASSEMBLER_H
#define TEMPLATE_RECIPE_ASSEMBLER_H

#include "product_recipe.h"
#include "recipe_selection.h"
#include "template_profile_assets.h"

#include <QMap>
#include <QString>
#include <QVector>

struct TemplateRecipeProfileSource
{
    RecipeProfile profile;
    TemplateProfileAssetManifest assetManifest;
};

struct TemplateRecipeAssembly
{
    ProductRecipe recipe;
    QMap<QString, QString> assetSourcePaths;
};

bool assembleTemplateProductRecipe(
        const ProductRecipe &recipeHeader,
        const QVector<TemplateRecipeProfileSource> &profileSources,
        TemplateRecipeAssembly *assembly,
        QString *errorMessage = nullptr);

bool assembleSelectedTemplateRecipe(
        const RecipeSelection &selection,
        TemplateRecipeAssembly *assembly,
        QString *errorMessage = nullptr);

bool replaceTemplateRecipeProfileAssets(
        TemplateRecipeAssembly *assembly,
        int profileIndex,
        const RecipeProfile &profile,
        const TemplateProfileAssetManifest &assetManifest,
        QString *errorMessage = nullptr);

#endif // TEMPLATE_RECIPE_ASSEMBLER_H
