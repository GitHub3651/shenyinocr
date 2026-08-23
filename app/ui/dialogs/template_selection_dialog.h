// 文件作用：统一显示单模板和多模板，并提供增加和批量移除入口。
#pragma once

#include "contracts/detection_mode.h"

#include <QDialog>
#include <QStringList>

class QPushButton;
class QTreeWidget;
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

    QStringList templatePaths() const;

private:
    QStringList checkedTemplatePaths() const;
    void addTemplateFolder();
    void removeCheckedTemplates();
    void addPath(const QString &path);
    void updateRemoveButtonState();
    void saveAndAccept();

    DetectionMode m_mode = DetectionMode::Stamp;
    TemplateApplicationService *m_templateService = nullptr;
    SettingsApplicationService *m_settingsService = nullptr;
    QTreeWidget *m_tree = nullptr;
    QPushButton *m_removeCheckedButton = nullptr;
};
