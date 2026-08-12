#ifndef RECIPES_TEMPLATE_RECIPE_DRAFT_SESSION_H
#define RECIPES_TEMPLATE_RECIPE_DRAFT_SESSION_H

#include "template_recipe_publisher.h"

class TemplateRecipeDraftSession
{
public:
    void reset();

    bool begin(const ProductRecipe &recipeHeader,
               const QString &sourceDirectoryPath,
               QString *errorMessage = nullptr);

    bool isActive() const;
    QString sourceDirectoryPath() const;
    const ProductRecipe &recipeHeader() const;

    bool publish(const RecipeStore &store,
                 const QString &sourceDirectoryPath,
                 const QVector<TemplateRecipeProfileSource> &profileSources,
                 RecipeSelection *publishedSelection,
                 QString *errorMessage = nullptr);

private:
    bool m_isActive = false;
    QString m_sourceDirectoryPath;
    ProductRecipe m_recipeHeader;
};

#endif // RECIPES_TEMPLATE_RECIPE_DRAFT_SESSION_H
