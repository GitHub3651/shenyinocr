#ifndef RECIPES_RECIPE_SELECTION_H
#define RECIPES_RECIPE_SELECTION_H

#include "recipe_store.h"

#include <QMap>
#include <QString>
#include <QVector>

struct ResolvedRecipeProfile
{
    RecipeProfile profile;
    QMap<QString, QString> assetPathsByRole;
};

struct RecipeSelection
{
    ProductRecipeSnapshot recipe;
    QString recipeDirectoryPath;
    QVector<ResolvedRecipeProfile> profiles;
};

bool loadRecipeSelection(const RecipeStore &store,
                         const QString &recipeId,
                         DetectionMode expectedMode,
                         RecipeSelection *selection,
                         QString *errorMessage = nullptr);

#endif // RECIPES_RECIPE_SELECTION_H
