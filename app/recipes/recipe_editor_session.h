#pragma once

#include "recipes/recipe_store.h"

#include <QMap>
#include <QString>

enum class RecipeEditorSessionState
{
    Inactive,
    New,
    Editing
};

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

private:
    QString m_workspacesRoot;
    QString m_sessionId;
    RecipeEditorSessionState m_state =
            RecipeEditorSessionState::Inactive;
    ProductRecipe m_recipe;
    QMap<QString, QString> m_assetSourcePaths;
};
