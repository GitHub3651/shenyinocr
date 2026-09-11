#include "ui/main_window/template/selection/template_selection_dialog.h"

#include "application/settings_application_service.h"
#include "application/template_application_service.h"
#include "system_support/logging/log_categories.h"
#include "ui_template_selection_dialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>

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
        TemplateApplicationService &templateService,
        SettingsApplicationService &settingsService,
        QWidget *parent)
    : QDialog(parent),
      ui(new Ui::TemplateSelectionDialog),
      m_mode(mode),
      m_templateService(templateService),
      m_settingsService(settingsService)
{
    ui->setupUi(this);
    ui->label_templateSelectionDescription->setText(
                multipleTemplatesAllowed(mode)
                ? QStringLiteral("勾选只用于选择要删除的模板。删除确认后立即生效，但不会删除磁盘模板文件夹。")
                : QStringLiteral("勾选只用于选择要删除的模板。当前模式最多应用一个模板，删除不会影响磁盘模板文件夹。"));
    ui->treeWidget_templateSelection->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->treeWidget_templateSelection->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->treeWidget_templateSelection->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->treeWidget_templateSelection->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    for (const QString &path : currentPaths) {
        addPath(path);
    }
    connect(ui->treeWidget_templateSelection, &QTreeWidget::itemChanged,
            this, [this](QTreeWidgetItem *, int column) {
        if (column == 0) {
            updateRemoveButtonState();
        }
    });

    connect(ui->pushButton_addTemplateFolder, &QPushButton::clicked,
            this, &TemplateSelectionDialog::addTemplateFolder);
    connect(ui->pushButton_removeCheckedTemplates, &QPushButton::clicked,
            this, &TemplateSelectionDialog::removeCheckedTemplates);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(
                QStringLiteral("确认应用"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setProperty(
                "uiRole", QStringLiteral("primary"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(
                QStringLiteral("取消"));
    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &TemplateSelectionDialog::saveAndAccept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    updateRemoveButtonState();
}

TemplateSelectionDialog::~TemplateSelectionDialog() = default;

QStringList TemplateSelectionDialog::templatePaths() const
{
    QStringList paths;
    for (int row = 0; row < ui->treeWidget_templateSelection->topLevelItemCount(); ++row) {
        const QTreeWidgetItem *item = ui->treeWidget_templateSelection->topLevelItem(row);
        paths.append(item->data(0, kPathRole).toString());
    }
    return paths;
}

QStringList TemplateSelectionDialog::checkedTemplatePaths() const
{
    QStringList paths;
    for (int row = 0; row < ui->treeWidget_templateSelection->topLevelItemCount(); ++row) {
        const QTreeWidgetItem *item = ui->treeWidget_templateSelection->topLevelItem(row);
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
    const TemplateSummary summary = m_templateService.readSummary(
                path, m_mode, &error);
    if (!summary.valid) {
        QMessageBox::warning(
                    this, QStringLiteral("模板不可用"),
                    error.userMessage.isEmpty()
                    ? QStringLiteral("所选文件夹不是当前模式的有效模板。")
                    : error.userMessage);
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
    const OperationResult result = m_settingsService.removeTemplatePaths(
                m_mode, paths);
    if (!result.isSuccess()) {
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=templates.selection_update_failed mode=%1 reason=%2")
                   .arg(detectionModeId(m_mode),
                        result.error.userMessage);
        QMessageBox::critical(
                    this, QStringLiteral("移除失败"),
                    result.error.userMessage.isEmpty()
                    ? QStringLiteral("无法从当前检测方案移除已勾选模板。")
                    : result.error.userMessage);
        return;
    }
    for (int row = ui->treeWidget_templateSelection->topLevelItemCount() - 1; row >= 0; --row) {
        if (ui->treeWidget_templateSelection->topLevelItem(row)->checkState(0) == Qt::Checked) {
            delete ui->treeWidget_templateSelection->takeTopLevelItem(row);
        }
    }
    updateRemoveButtonState();
    qCInfo(logTemplate).noquote()
            << QStringLiteral(
                "event=templates.selection_updated mode=%1 count=%2")
               .arg(detectionModeId(m_mode))
               .arg(ui->treeWidget_templateSelection->topLevelItemCount());
}

void TemplateSelectionDialog::addPath(const QString &path)
{
    const QString normalized = QDir::cleanPath(
                QFileInfo(path).absoluteFilePath());
    for (int row = 0; row < ui->treeWidget_templateSelection->topLevelItemCount(); ++row) {
        QTreeWidgetItem *existing = ui->treeWidget_templateSelection->topLevelItem(row);
        if (QString::compare(existing->data(0, kPathRole).toString(),
                             normalized, Qt::CaseInsensitive) == 0) {
            ui->treeWidget_templateSelection->setCurrentItem(existing);
            return;
        }
    }
    TemplateStoreError error;
    const TemplateSummary summary = m_templateService.readSummary(
                normalized, m_mode, &error);
    if (!multipleTemplatesAllowed(m_mode)) {
        ui->treeWidget_templateSelection->clear();
    }
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeWidget_templateSelection);
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
    ui->treeWidget_templateSelection->setCurrentItem(item);
}

void TemplateSelectionDialog::updateRemoveButtonState()
{
    ui->pushButton_removeCheckedTemplates->setEnabled(
                !checkedTemplatePaths().isEmpty());
}

void TemplateSelectionDialog::saveAndAccept()
{
    const OperationResult result = m_settingsService.saveTemplatePaths(
                m_mode, templatePaths());
    if (!result.isSuccess()) {
        qCWarning(logTemplate).noquote()
                << QStringLiteral(
                    "event=templates.selection_update_failed mode=%1 reason=%2")
                   .arg(detectionModeId(m_mode),
                        result.error.userMessage);
        QMessageBox::critical(
                    this, QStringLiteral("模板选择保存失败"),
                    result.error.userMessage.isEmpty()
                    ? QStringLiteral("无法保存当前模板选择。")
                    : result.error.userMessage);
        return;
    }
    qCInfo(logTemplate).noquote()
            << QStringLiteral(
                "event=templates.selection_updated mode=%1 count=%2")
               .arg(detectionModeId(m_mode))
               .arg(templatePaths().size());
    accept();
}
