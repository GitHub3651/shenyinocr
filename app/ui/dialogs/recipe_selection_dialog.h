#ifndef UI_DIALOGS_RECIPE_SELECTION_DIALOG_H
#define UI_DIALOGS_RECIPE_SELECTION_DIALOG_H

#include "recipes/recipe_store.h"

#include <QDialog>
#include <QVector>

class QDialogButtonBox;
class QLabel;
class QListWidget;

class RecipeSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RecipeSelectionDialog(
        const QVector<RecipeCatalogEntry> &recipes,
        QWidget *parent = nullptr);

    QString selectedRecipeId() const;

private:
    QListWidget *m_recipeList = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // UI_DIALOGS_RECIPE_SELECTION_DIALOG_H
