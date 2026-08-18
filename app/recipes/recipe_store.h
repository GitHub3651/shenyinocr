// 文件作用：本文件用于负责产品配方目录的校验、加载、事务保存和已发布配方枚举。
// 主要职责：负责产品配方目录的校验、加载、事务保存和已发布配方枚举。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef RECIPES_RECIPE_STORE_H
#define RECIPES_RECIPE_STORE_H

#include "product_recipe.h"
#include "prepared_recipe.h"

#include <QMap>
#include <QString>
#include <QVector>

#include <functional>

// 组件说明：RecipeCatalogEntry 数据结构集中保存该流程需要的一组相关数据。
struct RecipeCatalogEntry
{
    QString recipeId;
    QString displayName;
    DetectionMode detectionMode = DetectionMode::Stamp;
    int profileCount = 0;
};

// 组件说明：RecipeCatalogIssue 数据结构集中保存该流程需要的一组相关数据。
struct RecipeCatalogIssue
{
    QString directoryName;
    QString message;
};

// 组件说明：RecipeCatalog 数据结构集中保存该流程需要的一组相关数据。
struct RecipeCatalog
{
    QVector<RecipeCatalogEntry> recipes;
    QVector<RecipeCatalogIssue> invalidRecipes;
};

// 组件说明：RecipeStore 组件封装对应业务职责和生命周期边界。
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
