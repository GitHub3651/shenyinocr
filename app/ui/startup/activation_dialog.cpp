#include "ui/startup/activation_dialog.h"

#include "ui_activation_dialog.h"

#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

ActivationDialog::ActivationDialog(
    const RuntimeGuardResult &initialResult,
    QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ActivationDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    switch (initialResult.status) {
    case RuntimeGuardStatus::LicenseMissing:
        ui->label_status->setText(
                    QStringLiteral("软件尚未激活，请输入激活码。"));
        break;
    case RuntimeGuardStatus::LicenseInvalid:
        ui->label_status->setText(
                    QStringLiteral("软件许可证无效，请重新激活。"));
        break;
    case RuntimeGuardStatus::LicenseExpired:
        ui->label_status->setText(
                    QStringLiteral("软件许可证到期，请重新激活。"));
        break;
    case RuntimeGuardStatus::DeviceMismatch:
        ui->label_status->setText(
                    QStringLiteral(
                        "软件许可证与当前设备不匹配，请重新激活。"));
        break;
    default:
        ui->label_status->clear();
        break;
    }
    const QString requestCode = initialResult.activationRequestCode;
    ui->lineEdit_requestCode->setText(
                QStringLiteral("%1 %2 %3 %4")
                .arg(requestCode.mid(0, 4))
                .arg(requestCode.mid(4, 4))
                .arg(requestCode.mid(8, 4))
                .arg(requestCode.mid(12, 4)));

    connect(ui->pushButton_copyRequest,
            &QPushButton::clicked,
            this, &ActivationDialog::copyRequestCode);
    connect(ui->pushButton_activate,
            &QPushButton::clicked,
            this, &ActivationDialog::activateLicense);
    connect(ui->pushButton_exit,
            &QPushButton::clicked,
            this, &QDialog::reject);
    connect(ui->lineEdit_activationCode,
            &QLineEdit::returnPressed,
            this, &ActivationDialog::activateLicense);
    connect(ui->lineEdit_activationCode,
            &QLineEdit::textEdited,
            this, [this](const QString &text) {
        const int cursorPosition =
                ui->lineEdit_activationCode->cursorPosition();
        int digitsBeforeCursor = 0;
        for (int index = 0;
             index < cursorPosition && index < text.size();
             ++index) {
            const QChar character = text.at(index);
            if (character >= QLatin1Char('0')
                    && character <= QLatin1Char('9')) {
                ++digitsBeforeCursor;
            }
        }

        QString digits;
        for (const QChar character : text) {
            if (character >= QLatin1Char('0')
                    && character <= QLatin1Char('9')) {
                digits.append(character);
            }
        }
        digits = digits.left(24);
        if (digitsBeforeCursor > digits.size()) {
            digitsBeforeCursor = digits.size();
        }

        QString formatted;
        for (int index = 0; index < digits.size(); ++index) {
            if (index > 0 && index % 4 == 0) {
                formatted.append(QLatin1Char(' '));
            }
            formatted.append(digits.at(index));
        }

        int formattedCursorPosition = 0;
        if (digitsBeforeCursor > 0) {
            formattedCursorPosition = digitsBeforeCursor
                    + (digitsBeforeCursor - 1) / 4;
            if (digitsBeforeCursor % 4 == 0
                    && digitsBeforeCursor < digits.size()) {
                ++formattedCursorPosition;
            }
        }
        ui->lineEdit_activationCode->setText(formatted);
        ui->lineEdit_activationCode->setCursorPosition(
                    formattedCursorPosition);
    });
}

ActivationDialog::~ActivationDialog() = default;

RuntimeGuardResult ActivationDialog::activationResult() const
{
    return m_activationResult;
}

void ActivationDialog::copyRequestCode()
{
    QApplication::clipboard()->setText(
                ui->lineEdit_requestCode->text());
    QMessageBox::information(
                this,
                QStringLiteral("提示"),
                QStringLiteral("激活申请码复制成功。"));
}

void ActivationDialog::activateLicense()
{
    const QString code = ui->lineEdit_activationCode->text().trimmed();
    if (code.isEmpty()) {
        ui->label_status->setText(QStringLiteral("请输入激活码。"));
        return;
    }

    const RuntimeGuardResult result = RuntimeGuard::activate(code);
    if (result.status == RuntimeGuardStatus::Valid) {
        m_activationResult = result;
        accept();
        return;
    }
    switch (result.status) {
    case RuntimeGuardStatus::LicenseInvalid:
        ui->label_status->setText(
                    QStringLiteral("激活码无效，请确认后重试。"));
        break;
    case RuntimeGuardStatus::LicenseExpired:
        ui->label_status->setText(
                    QStringLiteral(
                        "激活码已到期，请联系供应商重新获取。"));
        break;
    case RuntimeGuardStatus::DeviceMismatch:
        ui->label_status->setText(
                    QStringLiteral(
                        "激活码与当前设备不匹配，请确认后重试。"));
        break;
    case RuntimeGuardStatus::LicenseSaveFailed:
        ui->label_status->setText(
                    QStringLiteral(
                        "许可证保存失败，请确认软件目录可写。"));
        break;
    case RuntimeGuardStatus::DeviceUnavailable:
        ui->label_status->setText(
                    QStringLiteral(
                        "无法获取本机设备信息，请联系供应商。"));
        break;
    default:
        ui->label_status->clear();
        break;
    }
}
