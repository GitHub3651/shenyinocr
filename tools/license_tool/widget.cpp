#include "widget.h"
#include "ui_widget.h"
#include "system_support/license/license_codec.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>

namespace {

QString utf8Text(const char *text)
{
    return QString::fromUtf8(text);
}

QString hexText(const char *hex)
{
    return QString::fromUtf8(QByteArray::fromHex(hex));
}

bool makeLicenseFile(const QDate &expiresDate, const QString &outputPath, QString *errorMessage)
{
    const LicenseFileError error = LicenseCodec::writeFile(
                expiresDate, outputPath);
    if (error != LicenseFileError::None) {
        if (errorMessage) {
            *errorMessage = error == LicenseFileError::InvalidDate
                    ? hexText("E697A5E69C9FE697A0E69588E38082")
                    : hexText("E58699E585A5E69687E4BBB6E5A4B1E8B4A5E38082");
        }
        return false;
    }

    return true;
}

QString licenseInfoText(const QString &filePath, QString *errorMessage)
{
    const LicenseReadResult result = LicenseCodec::readFile(filePath);
    if (!result.succeeded()) {
        if (errorMessage) {
            *errorMessage = result.error == LicenseFileError::FileReadFailed
                    ? hexText("E69687E4BBB6E8AFBBE58F96E5A4B1E8B4A5E68896E58685E5AEB9E4B8BAE7A9BAE38082")
                    : hexText("E69687E4BBB6E6A0BCE5BC8FE697A0E69588E38082");
        }
        return QString();
    }

    const QString status = QDate::currentDate() <= result.expiresDate
            ? hexText("E69C89E69588")
            : hexText("E5B7B2E8B685E8BF87");

    QString text;
    text += hexText("E69687E4BBB6E8B7AFE5BE84EFBC9A") + QDir::toNativeSeparators(filePath) + QStringLiteral("\n");
    text += hexText("E588B0E69C9FE697A5E69C9FEFBC9A") + result.expiresDate.toString(QStringLiteral("yyyy-MM-dd")) + QStringLiteral("\n");
    text += hexText("E697A5E69C9FE78AB6E68081EFBC9A") + status;
    return text;
}

} // namespace

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    ui->outputEdit->setText(defaultLicensePath());
    ui->datEdit->setText(defaultLicensePath());
    ui->licenseInfoEdit->setReadOnly(true);
    ui->expiresEdit->setDate(QDate::currentDate().addYears(1));
    ui->statusLabel->setText(hexText("E8AFB7E98089E68BA9E69C89E69588E69C9FE5B9B6E7949FE68890206C6963656E73652E696E69E38082"));

    connect(ui->browseOutputButton, &QPushButton::clicked,
            this, &Widget::browseOutputFile);
    connect(ui->generateButton, &QPushButton::clicked,
            this, &Widget::generateLicenseFile);
    connect(ui->browseDatButton, &QPushButton::clicked,
            this, &Widget::browseDatFile);
    connect(ui->readDatButton, &QPushButton::clicked,
            this, &Widget::readDatFile);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::browseOutputFile()
{
    const QString filePath = QFileDialog::getSaveFileName(
                this,
                hexText("E98089E68BA9E8BE93E587BAE69687E4BBB6"),
                ui->outputEdit->text(),
                QStringLiteral("INI Files (*.ini);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        ui->outputEdit->setText(QDir::toNativeSeparators(filePath));
    }
}

void Widget::generateLicenseFile()
{
    const QString outputPath = ui->outputEdit->text().trimmed();
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, utf8Text("\xE6\x8F\x90\xE7\xA4\xBA"),
                             hexText("E8AFB7E98089E68BA9E8BE93E587BAE69687E4BBB6E38082"));
        return;
    }

    QString errorMessage;
    if (!makeLicenseFile(ui->expiresEdit->date(), outputPath, &errorMessage)) {
        QMessageBox::critical(this, utf8Text("\xE6\x8F\x90\xE7\xA4\xBA"), errorMessage);
        ui->statusLabel->setText(errorMessage);
        return;
    }

    ui->statusLabel->setText(hexText("6C6963656E73652E696E6920E5B7B2E7949FE68890E38082"));
    ui->datEdit->setText(outputPath);
    readDatFile();
    QMessageBox::information(this, utf8Text("\xE6\x8F\x90\xE7\xA4\xBA"),
                             hexText("E7949FE68890E5AE8CE68890E38082"));
}

void Widget::browseDatFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
                this,
                hexText("E98089E68BA9206C6963656E73652E696E6920E69687E4BBB6"),
                QFileInfo(ui->datEdit->text()).absolutePath(),
                QStringLiteral("INI Files (*.ini);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        ui->datEdit->setText(QDir::toNativeSeparators(filePath));
    }
}

void Widget::readDatFile()
{
    const QString filePath = ui->datEdit->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, utf8Text("\xE6\x8F\x90\xE7\xA4\xBA"),
                             hexText("E8AFB7E98089E68BA9E69687E4BBB6E38082"));
        return;
    }

    QString errorMessage;
    const QString text = licenseInfoText(filePath, &errorMessage);
    if (text.isEmpty()) {
        ui->licenseInfoEdit->clear();
        QMessageBox::critical(this, utf8Text("\xE6\x8F\x90\xE7\xA4\xBA"), errorMessage);
        return;
    }

    ui->licenseInfoEdit->setPlainText(text);
}

QString Widget::defaultLicensePath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("license.ini"));
}
