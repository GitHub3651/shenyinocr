#ifndef RECIPES_RECIPE_SELECTION_H
#define RECIPES_RECIPE_SELECTION_H

#include "recipe_store.h"

#include <QMap>
#include <QString>
#include <QStringList>
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

struct RecipeSelectionIssue
{
    QString recipeId;
    QString message;
};

struct RecipeSelectionBatch
{
    QVector<RecipeSelection> selections;
    QVector<RecipeSelectionIssue> rejectedSelections;
};

bool loadRecipeSelection(const RecipeStore &store,
                         const QString &recipeId,
                         DetectionMode expectedMode,
                         RecipeSelection *selection,
                         QString *errorMessage = nullptr);

bool loadRecipeSelectionBatch(const RecipeStore &store,
                              const QStringList &recipeIds,
                              DetectionMode expectedMode,
                              RecipeSelectionBatch *batch,
                              QString *errorMessage = nullptr);

#endif // RECIPES_RECIPE_SELECTION_H
