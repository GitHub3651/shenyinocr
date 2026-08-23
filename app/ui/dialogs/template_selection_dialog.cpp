// 文件作用：实现模板路径展示、增加和批量引用移除。
#include "ui/dialogs/template_selection_dialog.h"

#include "application/settings_application_service.h"
#include "application/template_application_service.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
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
    setObjectName(QStringLiteral("templateSelectionDialog"));
    setWindowTitle(QStringLiteral("选择模板"));
    setModal(true);
    resize(860, 460);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *description = new QLabel(
                multipleTemplatesAllowed(mode)
                ? QStringLiteral("勾选只用于选择要删除的模板。删除确认后立即生效，但不会删除磁盘模板文件夹。")
                : QStringLiteral("勾选只用于选择要删除的模板。当前模式最多应用一个模板，删除不会影响磁盘模板文件夹。"),
                this);
    description->setObjectName(QStringLiteral("label_templateSelectionDescription"));
    description->setWordWrap(true);
    layout->addWidget(description);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName(QStringLiteral("treeWidget_templateSelection"));
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels(QStringList()
                            << QStringLiteral("待移除")
                            << QStringLiteral("模板名称")
                            << QStringLiteral("完整路径")
                            << QStringLiteral("状态"));
    m_tree->setRootIsDecorated(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(m_tree, 1);
    for (const QString &path : currentPaths) {
        addPath(path);
    }
    connect(m_tree, &QTreeWidget::itemChanged,
            this, [this](QTreeWidgetItem *, int column) {
        if (column == 0) {
            updateRemoveButtonState();
        }
    });

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *addButton = new QPushButton(
                QStringLiteral("增加模板文件夹"), this);
    addButton->setObjectName(QStringLiteral("pushButton_addTemplateFolder"));
    m_removeCheckedButton = new QPushButton(
                QStringLiteral("删除已勾选模板"), this);
    m_removeCheckedButton->setObjectName(
                QStringLiteral("pushButton_removeCheckedTemplates"));
    m_removeCheckedButton->setProperty(
                "uiRole", QStringLiteral("danger"));
    actions->addWidget(addButton);
    actions->addWidget(m_removeCheckedButton);
    actions->addStretch(1);
    connect(addButton, &QPushButton::clicked,
            this, [this]() { addTemplateFolder(); });
    connect(m_removeCheckedButton, &QPushButton::clicked,
            this, [this]() { removeCheckedTemplates(); });
    layout->addLayout(actions);

    QDialogButtonBox *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(
                QStringLiteral("确认应用"));
    buttons->button(QDialogButtonBox::Ok)->setProperty(
                "uiRole", QStringLiteral("primary"));
    buttons->button(QDialogButtonBox::Cancel)->setText(
                QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted,
            this, [this]() { saveAndAccept(); });
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    layout->addWidget(buttons);
    updateRemoveButtonState();
}

QStringList TemplateSelectionDialog::templatePaths() const
{
    QStringList paths;
    for (int row = 0; row < m_tree->topLevelItemCount(); ++row) {
        const QTreeWidgetItem *item = m_tree->topLevelItem(row);
        paths.append(item->data(0, kPathRole).toString());
    }
    return paths;
}

QStringList TemplateSelectionDialog::checkedTemplatePaths() const
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
                this, QStringLiteral("选择模板文件夹"), QString(),
                QFileDialog::ShowDirsOnly
                | QFileDialog::DontUseNativeDialog);
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
    addPath(path);
}

void TemplateSelectionDialog::removeCheckedTemplates()
{
    const QStringList paths = checkedTemplatePaths();
    if (paths.isEmpty()) {
        return;
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
                this, QStringLiteral("确认移除"),
                QStringLiteral("将从当前检测方案移除所有已勾选模板，磁盘模板文件夹不会删除。是否继续？"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }
    const OperationResult result = m_settingsService->removeTemplatePaths(
                m_mode, paths);
    if (!result.isSuccess()) {
        QMessageBox::critical(
                    this, QStringLiteral("移除失败"),
                    result.error.userMessage.isEmpty()
                    ? QStringLiteral("无法从当前检测方案移除已勾选模板。")
                    : result.error.userMessage);
        return;
    }
    for (int row = m_tree->topLevelItemCount() - 1; row >= 0; --row) {
        if (m_tree->topLevelItem(row)->checkState(0) == Qt::Checked) {
            delete m_tree->takeTopLevelItem(row);
        }
    }
    updateRemoveButtonState();
}

void TemplateSelectionDialog::addPath(const QString &path)
{
    const QString normalized = QDir::cleanPath(
                QFileInfo(path).absoluteFilePath());
    for (int row = 0; row < m_tree->topLevelItemCount(); ++row) {
        QTreeWidgetItem *existing = m_tree->topLevelItem(row);
        if (QString::compare(existing->data(0, kPathRole).toString(),
                             normalized, Qt::CaseInsensitive) == 0) {
            m_tree->setCurrentItem(existing);
            return;
        }
    }
    TemplateStoreError error;
    const TemplateSummary summary = m_templateService
            ? m_templateService->readSummary(normalized, m_mode, &error)
            : TemplateSummary();
    if (!multipleTemplatesAllowed(m_mode)) {
        m_tree->clear();
    }
    QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
    item->setData(0, kPathRole, normalized);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setText(1, QFileInfo(normalized).fileName());
    item->setText(2, normalized);
    item->setText(3, summary.valid
                  ? summary.message
                  : (error.userMessage.isEmpty()
                     ? QStringLiteral("模板无法读取")
                     : error.userMessage));
    if (!summary.valid) {
        item->setIcon(
                    3,
                    style()->standardIcon(QStyle::SP_MessageBoxWarning));
    }
    item->setCheckState(0, Qt::Unchecked);
    m_tree->setCurrentItem(item);
}

void TemplateSelectionDialog::updateRemoveButtonState()
{
    m_removeCheckedButton->setEnabled(
                !checkedTemplatePaths().isEmpty());
}

void TemplateSelectionDialog::saveAndAccept()
{
    if (!m_settingsService) {
        QMessageBox::critical(this, QStringLiteral("模板选择保存失败"),
                              QStringLiteral("设置服务不可用。"));
        return;
    }
    const OperationResult result = m_settingsService->saveTemplatePaths(
                m_mode, templatePaths());
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
