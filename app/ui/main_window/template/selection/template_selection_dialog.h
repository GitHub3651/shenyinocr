#pragma once

#include "contracts/detection_mode.h"

#include <QDialog>
#include <QStringList>

#include <memory>

class SettingsApplicationService;
class TemplateApplicationService;

namespace Ui {
class TemplateSelectionDialog;
}

class TemplateSelectionDialog : public QDialog
{
public:
    TemplateSelectionDialog(
        DetectionMode mode,
        const QStringList &currentPaths,
        TemplateApplicationService &templateService,
        SettingsApplicationService &settingsService,
        QWidget *parent = nullptr);
    ~TemplateSelectionDialog() override;

    QStringList templatePaths() const;

private:
    QStringList checkedTemplatePaths() const;
    void addTemplateFolder();
    void removeCheckedTemplates();
    void addPath(const QString &path);
    void updateRemoveButtonState();
    void saveAndAccept();

    std::unique_ptr<Ui::TemplateSelectionDialog> ui;
    DetectionMode m_mode = DetectionMode::Stamp;
    TemplateApplicationService &m_templateService;
    SettingsApplicationService &m_settingsService;
};
