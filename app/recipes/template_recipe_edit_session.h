#ifndef RECIPES_TEMPLATE_RECIPE_EDIT_SESSION_H
#define RECIPES_TEMPLATE_RECIPE_EDIT_SESSION_H

#include "template_recipe_assembler.h"

class TemplateRecipeEditSession
{
public:
    void reset();

    bool begin(const RecipeSelection &selection,
               QString *errorMessage = nullptr);

    bool isActive() const;
    const ProductRecipe &recipe() const;

    bool updateProfile(int profileIndex,
                       const RecipeProfile &profile,
                       QString *errorMessage = nullptr);

    bool updateProfiles(const QVector<RecipeProfile> &profiles,
                        QString *errorMessage = nullptr);

    bool replaceProfileAssets(
            int profileIndex,
            const RecipeProfile &profile,
            const TemplateProfileAssetManifest &assetManifest,
            QString *errorMessage = nullptr);

    bool publish(const RecipeStore &store,
                 RecipeSelection *publishedSelection,
                 QString *errorMessage = nullptr);

private:
    bool m_isActive = false;
    TemplateRecipeAssembly m_assembly;
};

#endif // RECIPES_TEMPLATE_RECIPE_EDIT_SESSION_H
