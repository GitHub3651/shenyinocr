#include "RuntimeGuard.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QStringList>
#include <QTextStream>

namespace {

const char kLicenseFileName[] = "license.ini";

QByteArray secretKey()
{
    return QByteArrayLiteral("AutoOCRproject.license.expire.v1.20260706");
}

QString licenseFilePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QLatin1String(kLicenseFileName));
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

QDate readLicenseExpiresDate()
{
    const QMap<QString, QString> values = readKeyValueFile(licenseFilePath());
    return expiresDateFromPayload(decryptLicensePayload(values.value(QStringLiteral("data"))));
}

} // namespace

bool RuntimeGuard::check()
{
    const QDate expiresDate = readLicenseExpiresDate();
    if (!expiresDate.isValid()) {
        return false;
    }

    return QDate::currentDate() <= expiresDate;
}
