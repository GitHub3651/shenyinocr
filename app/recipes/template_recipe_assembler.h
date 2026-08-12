#ifndef TEMPLATE_RECIPE_ASSEMBLER_H
#define TEMPLATE_RECIPE_ASSEMBLER_H

#include "product_recipe.h"
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

#endif // TEMPLATE_RECIPE_ASSEMBLER_H
