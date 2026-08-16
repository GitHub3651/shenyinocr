#ifndef RECIPES_RECIPE_STORE_H
#define RECIPES_RECIPE_STORE_H

#include "product_recipe.h"
#include "prepared_recipe.h"

#include <QMap>
#include <QString>
#include <QVector>

#include <functional>

struct RecipeCatalogEntry
{
    QString recipeId;
    QString displayName;
    DetectionMode detectionMode = DetectionMode::Stamp;
    int profileCount = 0;
};

struct RecipeCatalogIssue
{
    QString directoryName;
    QString message;
};

struct RecipeCatalog
{
    QVector<RecipeCatalogEntry> recipes;
    QVector<RecipeCatalogIssue> invalidRecipes;
};

class RecipeStore
{
public:
    typedef std::function<bool(const QString &sourceDirectoryPath,
                               const QString &destinationDirectoryPath)>
            DirectoryRenameFunction;

    explicit RecipeStore(const QString &recipesRootPath,
                         const DirectoryRenameFunction &directoryRenameFunction =
                         DirectoryRenameFunction());

    QString recipesRootPath() const;
    QString recipeDirectoryPath(const QString &recipeId) const;

    bool loadRecipe(const QString &recipeId,
                    ProductRecipe *recipe,
                    QString *errorMessage = nullptr) const;
    bool loadPreparedRecipe(
        const QString &recipeId,
        PreparedRecipeSnapshot *preparedRecipe,
        QString *errorMessage = nullptr) const;

    bool listRecipes(RecipeCatalog *catalog,
                     QString *errorMessage = nullptr) const;

    bool saveRecipe(const ProductRecipe &recipe,
                    const QMap<QString, QString> &assetSourcePaths,
                    QString *errorMessage = nullptr) const;

private:
    bool loadRecipeFromDirectory(const QString &directoryPath,
                                 const QString &expectedRecipeId,
                                 ProductRecipe *recipe,
                                 QString *errorMessage) const;
    bool renameDirectory(const QString &sourceDirectoryPath,
                         const QString &destinationDirectoryPath) const;

    QString m_recipesRootPath;
    DirectoryRenameFunction m_directoryRenameFunction;
};

#endif // RECIPES_RECIPE_STORE_H
