#pragma once

#include <QDialog>
#include <QString>

#include <memory>

namespace Ui {
class TemplateSaveDialog;
}

class TemplateSaveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TemplateSaveDialog(
        const QString &parentDirectory,
        QWidget *parent = nullptr);
    ~TemplateSaveDialog() override;

    QString templateName() const;
    QString parentDirectory() const;

private:
    void browseDirectory();
    void updateAcceptEnabled();
    void validateAndAccept();

    std::unique_ptr<Ui::TemplateSaveDialog> ui;
};
