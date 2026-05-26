#include "widget.h"
#include "ui_widget.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMap>
#include <QMessageAuthenticationCode>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QTextStream>
#include <QTextEdit>

namespace {

const char kVersion[] = "1";

QString utf8Text(const char *text)
{
    return QString::fromUtf8(text);
}

QByteArray secretKey()
{
    return QByteArrayLiteral("AutoOCRproject.syscache.v1.20260526");
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
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
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

QByteArray hmacSha256Hex(const QByteArray &data)
{
    return QMessageAuthenticationCode::hash(data, secretKey(), QCryptographicHash::Sha256).toHex();
}

QByteArray licensePayload(const QString &machineHash, const QString &expires)
{
    QByteArray payload;
    payload += "version=";
    payload += kVersion;
    payload += "\n";
    payload += "machine_hash=";
    payload += machineHash.toUtf8();
    payload += "\n";
    payload += "expires=";
    payload += expires.toUtf8();
    payload += "\n";
    return payload;
}

bool isHexSha256(const QString &value)
{
    if (value.size() != 64) {
        return false;
    }

    for (int i = 0; i < value.size(); ++i) {
        const QChar ch = value.at(i);
        const bool ok = (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))
                || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'));
        if (!ok) {
            return false;
        }
    }

    return true;
}

QString machineHashFromRequest(const QString &requestPath)
{
    const QMap<QString, QString> request = readKeyValueFile(requestPath);
    return request.value(QStringLiteral("machine_hash")).trimmed().toLower();
}

bool makeLicenseFile(const QString &requestPath, const QDate &expiresDate,
                     const QString &outputPath, QString *errorMessage)
{
    const QString machineHash = machineHashFromRequest(requestPath);
    if (!isHexSha256(machineHash)) {
        if (errorMessage) {
            *errorMessage = utf8Text("\xE8\xAF\xB7\xE6\xB1\x82\xE6\x96\x87\xE4\xBB\xB6\xE6\x97\xA0\xE6\x95\x88\xE3\x80\x82");
        }
        return false;
    }

    if (!expiresDate.isValid()) {
        if (errorMessage) {
            *errorMessage = utf8Text("\xE6\x97\xA5\xE6\x9C\x9F\xE6\x97\xA0\xE6\x95\x88\xE3\x80\x82");
        }
        return false;
    }

    const QString expires = expiresDate.toString(QStringLiteral("yyyy-MM-dd"));
    const QByteArray payload = licensePayload(machineHash, expires);
    QString content;
    content += QString::fromUtf8(payload);
    content += QStringLiteral("signature=%1\n").arg(QString::fromLatin1(hmacSha256Hex(payload)));

    if (!writeTextFile(outputPath, content)) {
        if (errorMessage) {
            *errorMessage = utf8Text("\xE5\x86\x99\xE5\x85\xA5\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5\xE3\x80\x82");
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
            *errorMessage = utf8Text("\xE6\x96\x87\xE4\xBB\xB6\xE8\xAF\xBB\xE5\x8F\x96\xE5\xA4\xB1\xE8\xB4\xA5\xE6\x88\x96\xE5\x86\x85\xE5\xAE\xB9\xE4\xB8\xBA\xE7\xA9\xBA\xE3\x80\x82");
        }
        return QString();
    }

    const QString version = values.value(QStringLiteral("version"));
    const QString machineHash = values.value(QStringLiteral("machine_hash"));
    const QString expires = values.value(QStringLiteral("expires"));
    const QString signature = values.value(QStringLiteral("signature"));

    const QByteArray payload = licensePayload(machineHash, expires);
    const QByteArray expectedSignature = hmacSha256Hex(payload);
    const bool signatureOk = !signature.isEmpty()
            && signature.toLatin1() == expectedSignature;

    const QDate expiresDate = QDate::fromString(expires, QStringLiteral("yyyy-MM-dd"));
    QString dateStatus = utf8Text("\xE6\x97\xA5\xE6\x9C\x9F\xE6\x97\xA0\xE6\x95\x88");
    if (expiresDate.isValid()) {
        dateStatus = QDate::currentDate() > expiresDate
                ? utf8Text("\xE5\xB7\xB2\xE8\xBF\x87\xE6\x9C\x9F")
                : utf8Text("\xE6\x9C\x89\xE6\x95\x88");
    }

    QString text;
    text += utf8Text("\xE6\x96\x87\xE4\xBB\xB6\xE8\xB7\xAF\xE5\xBE\x84\xEF\xBC\x9A") + QDir::toNativeSeparators(filePath) + QStringLiteral("\n");
    text += utf8Text("\xE6\x96\x87\xE4\xBB\xB6\xE7\x89\x88\xE6\x9C\xAC\xEF\xBC\x9A") + version + QStringLiteral("\n");
    text += utf8Text("\xE7\xBB\x91\xE5\xAE\x9A\xE6\x9C\xBA\xE5\x99\xA8\xE6\xA0\x87\xE8\xAF\x86\xEF\xBC\x9A") + machineHash + QStringLiteral("\n");
    text += utf8Text("\xE5\x88\xB0\xE6\x9C\x9F\xE6\x97\xA5\xE6\x9C\x9F\xEF\xBC\x9A") + expires + QStringLiteral("\n");
    text += utf8Text("\xE6\x96\x87\xE4\xBB\xB6\xE7\xAD\xBE\xE5\x90\x8D\xEF\xBC\x9A") + signature + QStringLiteral("\n");
    text += utf8Text("\xE7\xAD\xBE\xE5\x90\x8D\xE6\xA0\xA1\xE9\xAA\x8C\xEF\xBC\x9A")
            + (signatureOk ? utf8Text("\xE9\x80\x9A\xE8\xBF\x87\xEF\xBC\x88\xE6\x96\x87\xE4\xBB\xB6\xE6\x9C\xAA\xE8\xA2\xAB\xE4\xBF\xAE\xE6\x94\xB9\xEF\xBC\x89")
                           : utf8Text("\xE5\xA4\xB1\xE8\xB4\xA5\xEF\xBC\x88\xE6\x96\x87\xE4\xBB\xB6\xE5\x8F\xAF\xE8\x83\xBD\xE8\xA2\xAB\xE4\xBF\xAE\xE6\x94\xB9\xEF\xBC\x89"))
            + QStringLiteral("\n");
    text += utf8Text("\xE6\x97\xA5\xE6\x9C\x9F\xE7\x8A\xB6\xE6\x80\x81\xEF\xBC\x9A") + dateStatus + QStringLiteral("\n");
    text += utf8Text("\xE6\x8C\x89\xE5\xBD\x93\xE5\x89\x8D\xE5\x86\x85\xE5\xAE\xB9\xE8\xAE\xA1\xE7\xAE\x97\xE7\x9A\x84\xE7\xAD\xBE\xE5\x90\x8D\xEF\xBC\x9A")
            + QString::fromLatin1(expectedSignature);
    return text;
}

} // namespace

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    ui->requestEdit->setText(defaultCachePath(QStringLiteral("syscache.req")));
    ui->outputEdit->setText(defaultCachePath(QStringLiteral("syscache.dat")));
    ui->datEdit->setText(defaultCachePath(QStringLiteral("syscache.dat")));
    ui->machineHashEdit->setReadOnly(true);
    ui->licenseInfoEdit->setReadOnly(true);
    ui->expiresEdit->setDate(QDate::currentDate().addYears(1));

    connect(ui->requestEdit, &QLineEdit::textChanged, this,
            [this](const QString &) { refreshRequestInfo(); });
    connect(ui->browseRequestButton, &QPushButton::clicked,
            this, &Widget::browseRequestFile);
    connect(ui->browseOutputButton, &QPushButton::clicked,
            this, &Widget::browseOutputFile);
    connect(ui->generateButton, &QPushButton::clicked,
            this, &Widget::generateLicenseFile);
    connect(ui->browseDatButton, &QPushButton::clicked,
            this, &Widget::browseDatFile);
    connect(ui->readDatButton, &QPushButton::clicked,
            this, &Widget::readDatFile);

    refreshRequestInfo();
}

Widget::~Widget()
{
    delete ui;
}

void Widget::refreshRequestInfo()
{
    const QString machineHash = machineHashFromRequest(ui->requestEdit->text().trimmed());
    ui->machineHashEdit->setText(machineHash);

    if (isHexSha256(machineHash)) {
        ui->statusLabel->setText(utf8Text("\xE8\xAF\xB7\xE6\xB1\x82\xE6\x96\x87\xE4\xBB\xB6\xE5\xB7\xB2\xE8\xAF\xBB\xE5\x8F\x96\xE3\x80\x82"));
    } else {
        ui->statusLabel->setText(utf8Text("\xE8\xAF\xB7\xE6\xB1\x82\xE6\x96\x87\xE4\xBB\xB6\xE6\x9C\xAA\xE9\x80\x89\xE6\x8B\xA9\xE6\x88\x96\xE5\x86\x85\xE5\xAE\xB9\xE6\x97\xA0\xE6\x95\x88\xE3\x80\x82"));
    }
}

void Widget::browseRequestFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
                this,
                utf8Text("\xE9\x80\x89\xE6\x8B\xA9\xE8\xAF\xB7\xE6\xB1\x82\xE6\x96\x87\xE4\xBB\xB6"),
                QFileInfo(ui->requestEdit->text()).absolutePath(),
                QStringLiteral("Request Files (*.req);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return;
    }

    ui->requestEdit->setText(QDir::toNativeSeparators(filePath));
    ui->outputEdit->setText(QDir::toNativeSeparators(requestDirOutputPath(filePath)));
}

void Widget::browseOutputFile()
{
    const QString filePath = QFileDialog::getSaveFileName(
                this,
                utf8Text("\xE9\x80\x89\xE6\x8B\xA9\xE8\xBE\x93\xE5\x87\xBA\xE6\x96\x87\xE4\xBB\xB6"),
                ui->outputEdit->text(),
                QStringLiteral("Data Files (*.dat);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        ui->outputEdit->setText(QDir::toNativeSeparators(filePath));
    }
}

void Widget::generateLicenseFile()
{
    const QString requestPath = ui->requestEdit->text().trimmed();
    const QString outputPath = ui->outputEdit->text().trimmed();

    if (requestPath.isEmpty() || outputPath.isEmpty()) {
        QMessageBox::warning(this, utf8Text("\xE6\x8F\x90\xE7\xA4\xBA"),
                             utf8Text("\xE8\xAF\xB7\xE9\x80\x89\xE6\x8B\xA9\xE8\xBE\x93\xE5\x85\xA5\xE5\x92\x8C\xE8\xBE\x93\xE5\x87\xBA\xE6\x96\x87\xE4\xBB\xB6\xE3\x80\x82"));
        return;
    }

    QString errorMessage;
    if (!makeLicenseFile(requestPath, ui->expiresEdit->date(), outputPath, &errorMessage)) {
        QMessageBox::critical(this, utf8Text("\xE6\x8F\x90\xE7\xA4\xBA"), errorMessage);
        ui->statusLabel->setText(errorMessage);
        return;
    }

    ui->statusLabel->setText(utf8Text("\xE5\xB7\xB2\xE7\x94\x9F\xE6\x88\x90 syscache.dat\xE3\x80\x82"));
    ui->datEdit->setText(outputPath);
    readDatFile();
    QMessageBox::information(this, utf8Text("\xE6\x8F\x90\xE7\xA4\xBA"),
                             utf8Text("\xE7\x94\x9F\xE6\x88\x90\xE5\xAE\x8C\xE6\x88\x90\xE3\x80\x82"));
}

void Widget::browseDatFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
                this,
                utf8Text("\xE9\x80\x89\xE6\x8B\xA9 dat \xE6\x96\x87\xE4\xBB\xB6"),
                QFileInfo(ui->datEdit->text()).absolutePath(),
                QStringLiteral("Data Files (*.dat);;All Files (*.*)"));
    if (!filePath.isEmpty()) {
        ui->datEdit->setText(QDir::toNativeSeparators(filePath));
    }
}

void Widget::readDatFile()
{
    const QString filePath = ui->datEdit->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, utf8Text("\xE6\x8F\x90\xE7\xA4\xBA"),
                             utf8Text("\xE8\xAF\xB7\xE9\x80\x89\xE6\x8B\xA9 dat \xE6\x96\x87\xE4\xBB\xB6\xE3\x80\x82"));
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

QString Widget::defaultCachePath(const QString &fileName) const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("cache/%1").arg(fileName));
}

QString Widget::requestDirOutputPath(const QString &requestPath) const
{
    const QFileInfo requestInfo(requestPath);
    if (requestInfo.absoluteDir().exists()) {
        return requestInfo.absoluteDir().filePath(QStringLiteral("syscache.dat"));
    }

    return defaultCachePath(QStringLiteral("syscache.dat"));
}
