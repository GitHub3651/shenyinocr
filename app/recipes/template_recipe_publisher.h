#ifndef RECIPES_TEMPLATE_RECIPE_PUBLISHER_H
#define RECIPES_TEMPLATE_RECIPE_PUBLISHER_H

#include "recipe_selection.h"
#include "template_recipe_assembler.h"

bool publishTemplateRecipe(
        const RecipeStore &store,
        const ProductRecipe &recipeHeader,
        const QVector<TemplateRecipeProfileSource> &profileSources,
        RecipeSelection *publishedSelection,
        QString *errorMessage = nullptr);

#endif // RECIPES_TEMPLATE_RECIPE_PUBLISHER_H
