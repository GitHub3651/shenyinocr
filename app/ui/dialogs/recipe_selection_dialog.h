// 文件作用：本文件用于显示已发布配方并让用户按检测模式选择目标配方。
// 主要职责：显示已发布配方并让用户按检测模式选择目标配方。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef UI_DIALOGS_RECIPE_SELECTION_DIALOG_H
#define UI_DIALOGS_RECIPE_SELECTION_DIALOG_H

#include "application/template_editor_contract.h"

#include <QDialog>
#include <QVector>

class QDialogButtonBox;
class QLabel;
// 组件说明：QListWidget 组件封装本文件中与其名称对应的单一职责。
class QListWidget;

// 组件说明：RecipeSelectionDialog 组件负责对应界面区域的显示和用户交互。
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
