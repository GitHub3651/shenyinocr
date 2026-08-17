#ifndef UI_DIALOGS_RECIPE_SELECTION_DIALOG_H
#define UI_DIALOGS_RECIPE_SELECTION_DIALOG_H

#include "application/template_editor_contract.h"

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
        const QVector<TemplateRecipeCatalogEntry> &recipes,
        QWidget *parent = nullptr);

    QString selectedRecipeId() const;

private:
    QListWidget *m_recipeList = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // UI_DIALOGS_RECIPE_SELECTION_DIALOG_H
