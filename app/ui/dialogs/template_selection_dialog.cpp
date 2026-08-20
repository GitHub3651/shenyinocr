// 文件作用：实现模板路径勾选、增加外部文件夹、排序和单选约束。
#include "ui/dialogs/template_selection_dialog.h"

#include "application/settings_application_service.h"
#include "application/template_application_service.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

const int kPathRole = Qt::UserRole + 1;

bool multipleTemplatesAllowed(DetectionMode mode)
{
    return detectionModeDescriptor(mode).trackingKind
            == DetectionTrackingKind::MultipleTemplates;
}

}

TemplateSelectionDialog::TemplateSelectionDialog(
        DetectionMode mode,
        const QStringList &currentPaths,
        TemplateApplicationService *templateService,
        SettingsApplicationService *settingsService,
        QWidget *parent)
    : QDialog(parent),
      m_mode(mode),
      m_templateService(templateService),
      m_settingsService(settingsService)
{
    setWindowTitle(QStringLiteral("选择模板"));
    setModal(true);
    resize(860, 460);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *description = new QLabel(
                multipleTemplatesAllowed(mode)
                ? QStringLiteral("已应用模板会显示勾选。可以取消、增加或调整多个模板的顺序。")
                : QStringLiteral("已应用模板会显示勾选。当前模式最多应用一个模板。"),
                this);
    description->setWordWrap(true);
    layout->addWidget(description);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(5);
    m_tree->setHeaderLabels(QStringList()
                            << QStringLiteral("选择")
                            << QStringLiteral("顺序")
                            << QStringLiteral("模板名称")
                            << QStringLiteral("完整路径")
                            << QStringLiteral("状态"));
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setRootIsDecorated(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    layout->addWidget(m_tree, 1);
    for (const QString &path : currentPaths) {
        addPath(path, true);
    }
    connect(m_tree, &QTreeWidget::itemChanged,
            this, [this](QTreeWidgetItem *item, int column) {
        handleItemChanged(item, column);
    });

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *addButton = new QPushButton(
                QStringLiteral("增加模板文件夹"), this);
    QPushButton *upButton = new QPushButton(
                QStringLiteral("上移"), this);
    QPushButton *downButton = new QPushButton(
                QStringLiteral("下移"), this);
    actions->addWidget(addButton);
    actions->addWidget(upButton);
    actions->addWidget(downButton);
    actions->addStretch(1);
    connect(addButton, &QPushButton::clicked,
            this, [this]() { addTemplateFolder(); });
    connect(upButton, &QPushButton::clicked,
            this, [this]() { moveCurrentItem(-1); });
    connect(downButton, &QPushButton::clicked,
            this, [this]() { moveCurrentItem(1); });
    layout->addLayout(actions);

    QDialogButtonBox *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(
                QStringLiteral("确认应用"));
    buttons->button(QDialogButtonBox::Cancel)->setText(
                QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted,
            this, [this]() { saveAndAccept(); });
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    layout->addWidget(buttons);
    refreshOrderColumn();
}

QStringList TemplateSelectionDialog::selectedTemplatePaths() const
{
    QStringList paths;
    for (int row = 0; row < m_tree->topLevelItemCount(); ++row) {
        const QTreeWidgetItem *item = m_tree->topLevelItem(row);
        if (item->checkState(0) == Qt::Checked) {
            paths.append(item->data(0, kPathRole).toString());
        }
    }
    return paths;
}

void TemplateSelectionDialog::addTemplateFolder()
{
    const QString path = QFileDialog::getExistingDirectory(
                this, QStringLiteral("选择模板文件夹"));
    if (path.isEmpty()) {
        return;
    }
    TemplateStoreError error;
    const TemplateSummary summary = m_templateService
            ? m_templateService->readSummary(path, m_mode, &error)
            : TemplateSummary();
    if (!summary.valid) {
        QMessageBox::warning(
                    this, QStringLiteral("模板不可用"),
                    (error.userMessage.isEmpty()
                     ? QStringLiteral("所选文件夹不是当前模式的有效模板。")
                     : error.userMessage)
                    + QStringLiteral("\n\n")
                    + QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
        return;
    }
    addPath(path, true);
}

void TemplateSelectionDialog::addPath(
        const QString &path,
        bool checked)
{
    const QString normalized = QDir::cleanPath(
                QFileInfo(path).absoluteFilePath());
    for (int row = 0; row < m_tree->topLevelItemCount(); ++row) {
        QTreeWidgetItem *existing = m_tree->topLevelItem(row);
        if (QString::compare(existing->data(0, kPathRole).toString(),
                             normalized, Qt::CaseInsensitive) == 0) {
            existing->setCheckState(0,
                                    checked ? Qt::Checked : Qt::Unchecked);
            m_tree->setCurrentItem(existing);
            return;
        }
    }
    TemplateStoreError error;
    const TemplateSummary summary = m_templateService
            ? m_templateService->readSummary(normalized, m_mode, &error)
            : TemplateSummary();
    QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
    item->setData(0, kPathRole, normalized);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setText(2, QFileInfo(normalized).fileName());
    item->setText(3, normalized);
    item->setText(4, summary.valid
                  ? summary.message
                  : (error.userMessage.isEmpty()
                     ? QStringLiteral("模板无法读取")
                     : error.userMessage));
    item->setToolTip(3, normalized);
    if (!summary.valid) {
        item->setForeground(4, QBrush(Qt::red));
    }
    m_updating = true;
    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    m_updating = false;
    m_tree->setCurrentItem(item);
    if (checked) {
        handleItemChanged(item, 0);
    }
    refreshOrderColumn();
}

void TemplateSelectionDialog::handleItemChanged(
        QTreeWidgetItem *changed,
        int column)
{
    if (m_updating || !changed || column != 0) {
        return;
    }
    if (changed->checkState(0) == Qt::Checked
            && !multipleTemplatesAllowed(m_mode)) {
        m_updating = true;
        for (int row = 0; row < m_tree->topLevelItemCount(); ++row) {
            QTreeWidgetItem *item = m_tree->topLevelItem(row);
            if (item != changed) {
                item->setCheckState(0, Qt::Unchecked);
            }
        }
        m_updating = false;
    }
    refreshOrderColumn();
}

void TemplateSelectionDialog::moveCurrentItem(int offset)
{
    QTreeWidgetItem *item = m_tree->currentItem();
    const int row = m_tree->indexOfTopLevelItem(item);
    const int target = row + offset;
    if (!item || row < 0 || target < 0
            || target >= m_tree->topLevelItemCount()) {
        return;
    }
    m_tree->takeTopLevelItem(row);
    m_tree->insertTopLevelItem(target, item);
    m_tree->setCurrentItem(item);
    refreshOrderColumn();
}

void TemplateSelectionDialog::refreshOrderColumn()
{
    int selectedOrder = 0;
    for (int row = 0; row < m_tree->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = m_tree->topLevelItem(row);
        if (item->checkState(0) == Qt::Checked) {
            item->setText(1, QString::number(++selectedOrder));
        } else {
            item->setText(1, QStringLiteral("--"));
        }
    }
}

void TemplateSelectionDialog::saveAndAccept()
{
    if (!m_settingsService) {
        QMessageBox::critical(this, QStringLiteral("模板选择保存失败"),
                              QStringLiteral("设置服务不可用。"));
        return;
    }
    const OperationResult result = m_settingsService->saveTemplatePaths(
                m_mode, selectedTemplatePaths());
    if (!result.isSuccess()) {
        QMessageBox::critical(
                    this, QStringLiteral("模板选择保存失败"),
                    result.error.userMessage.isEmpty()
                    ? QStringLiteral("无法保存当前模板选择。")
                    : result.error.userMessage);
        return;
    }
    accept();
}
