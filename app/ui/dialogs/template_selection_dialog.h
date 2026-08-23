// 文件作用：统一显示单模板和多模板选择，并预勾选当前已应用路径。
#pragma once

#include "contracts/detection_mode.h"

#include <QDialog>
#include <QStringList>

class QTreeWidget;
class QTreeWidgetItem;
class SettingsApplicationService;
class TemplateApplicationService;

class TemplateSelectionDialog : public QDialog
{
public:
    TemplateSelectionDialog(
        DetectionMode mode,
        const QStringList &currentPaths,
        TemplateApplicationService *templateService,
        SettingsApplicationService *settingsService,
        QWidget *parent = nullptr);

    QStringList selectedTemplatePaths() const;

private:
    void addTemplateFolder();
    void removeCurrentTemplate();
    void addPath(const QString &path, bool checked);
    void handleItemChanged(QTreeWidgetItem *changed, int column);
    void moveCurrentItem(int offset);
    void refreshOrderColumn();
    void saveAndAccept();

    DetectionMode m_mode = DetectionMode::Stamp;
    TemplateApplicationService *m_templateService = nullptr;
    SettingsApplicationService *m_settingsService = nullptr;
    QTreeWidget *m_tree = nullptr;
    bool m_updating = false;
};
