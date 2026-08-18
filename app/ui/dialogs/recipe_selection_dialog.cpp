#include "recipe_selection_dialog.h"
// 文件作用：本文件用于显示已发布配方并让用户按检测模式选择目标配方。
// 主要职责：显示已发布配方并让用户按检测模式选择目标配方。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

const int kRecipeIdRole = Qt::UserRole + 1;

} // namespace

// 函数说明：RecipeSelectionDialog 构造函数创建组件并初始化其依赖和初始状态。
RecipeSelectionDialog::RecipeSelectionDialog(
        const QVector<TemplateRecipeCatalogEntry> &recipes,
        QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral(
                       "\u9009\u62E9\u5DF2\u53D1\u5E03\u914D\u65B9"));
    setModal(true);
    resize(520, 360);

    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *descriptionLabel = new QLabel(
                QStringLiteral(
                    "\u8BF7\u9009\u62E9\u5F53\u524D\u8BC6\u522B\u6A21\u5F0F"
                    "\u8981\u52A0\u8F7D\u7684\u4EA7\u54C1\u914D\u65B9\u3002"),
                this);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    m_recipeList = new QListWidget(this);
    m_recipeList->setSelectionMode(QAbstractItemView::SingleSelection);
    for (const TemplateRecipeCatalogEntry &recipe : recipes) {
        const QString displayName = recipe.displayName.trimmed().isEmpty()
                ? recipe.recipeId
                : recipe.displayName.trimmed();
        QListWidgetItem *item = new QListWidgetItem(
                    QStringLiteral("%1  \uFF08%2 \u4E2AProfile\uFF09")
                    .arg(displayName)
                    .arg(recipe.profileCount),
                    m_recipeList);
        item->setData(kRecipeIdRole, recipe.recipeId);
        item->setToolTip(recipe.recipeId);
    }
    if (m_recipeList->count() > 0) {
        m_recipeList->setCurrentRow(0);
    }
    layout->addWidget(m_recipeList, 1);

    m_buttonBox = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(
                QStringLiteral("\u52A0\u8F7D\u914D\u65B9"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(
                QStringLiteral("\u53D6\u6D88"));
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
                m_recipeList->currentItem() != nullptr);
    connect(m_recipeList,
            &QListWidget::itemSelectionChanged,
            this,
            [this]() {
        m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
                    m_recipeList->currentItem() != nullptr);
    });
    connect(m_recipeList,
            &QListWidget::itemDoubleClicked,
            this,
            [this](QListWidgetItem *) {
        if (m_recipeList->currentItem()) {
            accept();
        }
    });
    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    layout->addWidget(m_buttonBox);
}

// 函数说明：selectedRecipeId 函数读取、等待或计算对应的数据。
QString RecipeSelectionDialog::selectedRecipeId() const
{
    const QListWidgetItem *item = m_recipeList
            ? m_recipeList->currentItem()
            : nullptr;
    return item ? item->data(kRecipeIdRole).toString() : QString();
}
