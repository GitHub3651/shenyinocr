#include "widget.h"
#include "ui_widget.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QStringList>
#include <QTextEdit>
#include <QTextStream>

namespace {

QString utf8Text(const char *text)
{
    return QString::fromUtf8(text);
}

QString hexText(const char *hex)
{
    return QString::fromUtf8(QByteArray::fromHex(hex));
}

QByteArray secretKey()
{
    return QByteArrayLiteral("AutoOCRproject.license.expire.v1.20260706");
}

QByteArray cryptData(const QByteArray &data)
{
    const QByteArray key = QCryptographicHash::hash(secretKey(), QCryptographicHash::Sha256);
    QByteArray result;
    result.reserve(data.size());
    for (int i = 0; i < data.size(); ++i) {
        result.append(static_cast<char>(data.at(i) ^ key.at(i % key.size())));
    }
    return result;
}

QString encryptedPayloadForDate(const QDate &expiresDate)
{
    QByteArray payload;
    payload += "expires=";
    payload += expiresDate.toString(QStringLiteral("yyyy-MM-dd")).toUtf8();
    payload += "\n";
    return QString::fromLatin1(cryptData(payload).toBase64());
}

QString decryptLicensePayload(const QString &encryptedText)
{
    if (encryptedText.trimmed().isEmpty()) {
        return QString();
    }

    const QByteArray encrypted = QByteArray::fromBase64(encryptedText.trimmed().toLatin1());
    if (encrypted.isEmpty()) {
        return QString();
    }

    return QString::fromUtf8(cryptData(encrypted));
}

QMap<QString, QString> readKeyValueFile(const QString &filePath)
{
    QMap<QString, QString> values;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return values;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))
                || line.startsWith(QLatin1Char('['))) {
            continue;
        }

        const int pos = line.indexOf(QLatin1Char('='));
        if (pos <= 0) {
            continue;
        }

        values.insert(line.left(pos).trimmed(), line.mid(pos + 1).trimmed());
    }

    return values;
}

bool writeTextFile(const QString &filePath, const QString &content)
{
    const QFileInfo fileInfo(filePath);
    const QDir dir = fileInfo.absoluteDir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        return false;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    file.write(content.toUtf8());
    return file.commit();
}

QDate expiresDateFromPayload(const QString &payload)
{
    const QStringList lines = payload.split(QLatin1Char('\n'), QString::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QStringLiteral("expires="))) {
            continue;
        }

        const QString expires = trimmed.mid(QStringLiteral("expires=").size()).trimmed();
        return QDate::fromString(expires, QStringLiteral("yyyy-MM-dd"));
    }

    return QDate();
}

bool makeLicenseFile(const QDate &expiresDate, const QString &outputPath, QString *errorMessage)
{
    if (!expiresDate.isValid()) {
        if (errorMessage) {
            *errorMessage = hexText("E697A5E69C9FE697A0E69588E38082");
        }
        return false;
    }

    QString content;
    content += QStringLiteral("[License]\n");
    content += QStringLiteral("data=%1\n").arg(encryptedPayloadForDate(expiresDate));

    if (!writeTextFile(outputPath, content)) {
        if (errorMessage) {
            *errorMessage = hexText("E58699E585A5E69687E4BBB6E5A4B1E8B4A5E38082");
        }
        return false;
    }

    return true;
}

QString licenseInfoText(const QString &filePath, QString *errorMessage)
{
    const QMap<QString, QString> values = readKeyValueFile(filePath);
    if (values.isEmpty()) {
        if (errorMessage) {
            *errorMessage = hexText("E69687E4BBB6E8AFBBE58F96E5A4B1E8B4A5E68896E58685E5AEB9E4B8BAE7A9BAE38082");
        }
        return QString();
    }

    const QDate expiresDate = expiresDateFromPayload(
                decryptLicensePayload(values.value(QStringLiteral("data"))));
    if (!expiresDate.isValid()) {
        if (errorMessage) {
            *errorMessage = hexText("E69687E4BBB6E6A0BCE5BC8FE697A0E69588E38082");
        }
        return QString();
    }

    const QString status = QDate::currentDate() <= expiresDate
            ? hexText("E69C89E69588")
            : hexText("E5B7B2E8B685E8BF87");

    QString text;
    text += hexText("E69687E4BBB6E8B7AFE5BE84EFBC9A") + QDir::toNativeSeparators(filePath) + QStringLiteral("\n");
    text += hexText("E588B0E69C9FE697A5E69C9FEFBC9A") + expiresDate.toString(QStringLiteral("yyyy-MM-dd")) + QStringLiteral("\n");
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
