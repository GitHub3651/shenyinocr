// 文件作用：本文件用于维护模板编辑事务、草稿状态、资源暂存和提交取消边界。
// 主要职责：维护模板编辑事务和工作区中的配方资源。
// 模块位置：配方层；负责产品参数、资源和编辑事务，不依赖界面或检测实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "recipes/recipe_store.h"

#include <QImage>
#include <QMap>
#include <QString>

#include <opencv2/core.hpp>

#include <vector>

// 模板首次保存时需要写入编辑工作区的一组原始资源。
struct InitialRecipeProfileAssets
{
    int profileIndex = 0;
    cv::Mat rawImage;
    cv::Rect trackingImageRect;
    std::vector<cv::Point2f> stampPolygon;
    std::vector<cv::Point2f> datePolygon;
    std::vector<cv::Point2f> barcodePolygon;
    cv::Mat stampRing;
};

// 组件说明：RecipeEditorSessionState 枚举列出该组件允许使用的稳定状态和选项。
enum class RecipeEditorSessionState
{
    Inactive,
    New,
    Editing
};

// 组件说明：RecipeEditorSession 组件封装对应业务职责和生命周期边界。
class RecipeEditorSession
{
public:
    explicit RecipeEditorSession(
        const QString &editorWorkspacesRootPath = QString());
    ~RecipeEditorSession();

    void reset();
    bool beginNew(const ProductRecipe &recipe,
                  QString *errorMessage = nullptr);
    bool beginEdit(const RecipeStore &store,
                   const QString &recipeId,
                   QString *errorMessage = nullptr);

    bool isActive() const;
    RecipeEditorSessionState state() const;
    QString sessionId() const;
    QString workspacePath() const;
    const ProductRecipe &recipe() const;
    QMap<QString, QString> assetSourcePaths() const;

    bool replaceDraft(const ProductRecipe &recipe,
                      const QMap<QString, QString> &assetSourcePaths,
                      QString *errorMessage = nullptr);
    bool updateProfile(int profileIndex,
                       const RecipeProfile &profile,
                       QString *errorMessage = nullptr);
    bool publish(const RecipeStore &store,
                 PreparedRecipeSnapshot *preparedRecipe,
                 QString *errorMessage = nullptr);

    bool stageInitialProfileAssets(
        const InitialRecipeProfileAssets &assets,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage = nullptr) const;
    bool stageCharacterAssets(
        int profileIndex,
        const QMap<QString, QImage> &characterImages,
        ProductRecipe *recipe,
        RecipeProfile *profile,
        QMap<QString, QString> *assetSourcePaths,
        QString *errorMessage = nullptr) const;

private:
    QString m_workspacesRoot;
    QString m_sessionId;
    RecipeEditorSessionState m_state =
            RecipeEditorSessionState::Inactive;
    ProductRecipe m_recipe;
    QMap<QString, QString> m_assetSourcePaths;
};
