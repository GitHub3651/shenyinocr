// 文件作用：实现产品模板保存位置对话框。
#include "ui/main_window/template/save/template_save_dialog.h"

#include "ui_template_save_dialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>

TemplateSaveDialog::TemplateSaveDialog(
    const QString &parentDirectory,
    QWidget *parent)
    : QDialog(parent),
      ui(new Ui::TemplateSaveDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(
                QStringLiteral("确定"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(
                QStringLiteral("取消"));
    ui->lineEdit_parentDirectory->setText(
                QDir(parentDirectory).absolutePath());
    connect(ui->pushButton_browseDirectory,
            &QPushButton::clicked,
            this, &TemplateSaveDialog::browseDirectory);
    connect(ui->lineEdit_templateName,
            &QLineEdit::textChanged,
            this, &TemplateSaveDialog::updateAcceptEnabled);
    connect(ui->buttonBox,
            &QDialogButtonBox::accepted,
            this, &TemplateSaveDialog::validateAndAccept);
    connect(ui->buttonBox,
            &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    updateAcceptEnabled();
}

TemplateSaveDialog::~TemplateSaveDialog() = default;

QString TemplateSaveDialog::templateName() const
{
    return ui->lineEdit_templateName->text().trimmed();
}

QString TemplateSaveDialog::parentDirectory() const
{
    return QDir(ui->lineEdit_parentDirectory->text()).absolutePath();
}

void TemplateSaveDialog::browseDirectory()
{
    const QString selected = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("选择模板保存目录"),
                ui->lineEdit_parentDirectory->text(),
                QFileDialog::ShowDirsOnly
                | QFileDialog::DontUseNativeDialog);
    if (!selected.isEmpty()) {
        ui->lineEdit_parentDirectory->setText(
                    QDir(selected).absolutePath());
    }
}

void TemplateSaveDialog::updateAcceptEnabled()
{
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
                !templateName().isEmpty());
}

void TemplateSaveDialog::validateAndAccept()
{
    const QString name = templateName();
    const QRegularExpression invalidChars(
                QStringLiteral(R"([\\/:*?"<>|])"));
    if (name == QLatin1String(".") || name == QLatin1String("..")
            || name.contains(invalidChars)) {
        QMessageBox::warning(
                    this,
                    QStringLiteral("模板名称无效"),
                    QStringLiteral("模板名称不能是“.”或“..”，也不能包含 "
                                   "\\ / : * ? \" < > |。"));
        return;
    }
    if (!QFileInfo(ui->lineEdit_parentDirectory->text()).isDir()) {
        QMessageBox::warning(
                    this,
                    QStringLiteral("模板保存目录无效"),
                    QStringLiteral("当前模板保存目录不存在，请重新选择。"));
        return;
    }
    accept();
}
