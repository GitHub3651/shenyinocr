#ifndef RECIPES_RECIPE_STORE_H
#define RECIPES_RECIPE_STORE_H

#include "product_recipe.h"

#include <QMap>
#include <QString>

#include <functional>

class RecipeStore
{
public:
    typedef std::function<bool(const ProductRecipe &recipe,
                               const QString &assetKey,
                               const QString &absoluteAssetPath,
                               QString *errorMessage)> AssetValidator;
    typedef std::function<bool(const QString &sourceDirectoryPath,
                               const QString &destinationDirectoryPath)>
            DirectoryRenameFunction;

    explicit RecipeStore(const QString &recipesRootPath,
                         const AssetValidator &assetValidator = AssetValidator(),
                         const DirectoryRenameFunction &directoryRenameFunction =
                         DirectoryRenameFunction());

    QString recipesRootPath() const;
    QString recipeDirectoryPath(const QString &recipeId) const;

    bool loadRecipe(const QString &recipeId,
                    ProductRecipe *recipe,
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
    AssetValidator m_assetValidator;
    DirectoryRenameFunction m_directoryRenameFunction;
};

#endif // RECIPES_RECIPE_STORE_H
