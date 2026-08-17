#include "recipe_selection_dialog.h"

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

QString RecipeSelectionDialog::selectedRecipeId() const
{
    const QListWidgetItem *item = m_recipeList
            ? m_recipeList->currentItem()
            : nullptr;
    return item ? item->data(kRecipeIdRole).toString() : QString();
}
