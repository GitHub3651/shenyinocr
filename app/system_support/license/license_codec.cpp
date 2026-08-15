#include "system_support/license/license_codec.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>

namespace {

QByteArray secretKey()
{
    return QByteArrayLiteral("AutoOCRproject.license.expire.v1.20260706");
}

QByteArray cryptData(const QByteArray &data)
{
    const QByteArray key = QCryptographicHash::hash(
                secretKey(), QCryptographicHash::Sha256);
    QByteArray result;
    result.reserve(data.size());
    for (int i = 0; i < data.size(); ++i) {
        result.append(static_cast<char>(
                          data.at(i) ^ key.at(i % key.size())));
    }
    return result;
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
        if (line.isEmpty()
                || line.startsWith(QLatin1Char('#'))
                || line.startsWith(QLatin1Char('['))) {
            continue;
        }

        const int separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            continue;
        }
        values.insert(line.left(separator).trimmed(),
                      line.mid(separator + 1).trimmed());
    }
    return values;
}

QString decryptPayload(const QString &encryptedText)
{
    if (encryptedText.trimmed().isEmpty()) {
        return QString();
    }

    const QByteArray encrypted = QByteArray::fromBase64(
                encryptedText.trimmed().toLatin1());
    if (encrypted.isEmpty()) {
        return QString();
    }
    return QString::fromUtf8(cryptData(encrypted));
}

QDate expiresDateFromPayload(const QString &payload)
{
    const QStringList lines = payload.split(
                QLatin1Char('\n'), QString::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QStringLiteral("expires="))) {
            continue;
        }
        return QDate::fromString(
                    trimmed.mid(QStringLiteral("expires=").size()).trimmed(),
                    QStringLiteral("yyyy-MM-dd"));
    }
    return QDate();
}

QString encryptedPayloadForDate(const QDate &expiresDate)
{
    QByteArray payload;
    payload += "expires=";
    payload += expiresDate.toString(
                QStringLiteral("yyyy-MM-dd")).toUtf8();
    payload += "\n";
    return QString::fromLatin1(cryptData(payload).toBase64());
}

} // namespace

bool LicenseReadResult::succeeded() const
{
    return error == LicenseFileError::None && expiresDate.isValid();
}

LicenseReadResult LicenseCodec::readFile(const QString &filePath)
{
    LicenseReadResult result;
    const QMap<QString, QString> values = readKeyValueFile(filePath);
    if (values.isEmpty()) {
        result.error = LicenseFileError::FileReadFailed;
        return result;
    }

    result.expiresDate = expiresDateFromPayload(
                decryptPayload(values.value(QStringLiteral("data"))));
    if (!result.expiresDate.isValid()) {
        result.error = LicenseFileError::InvalidFormat;
        return result;
    }

    result.error = LicenseFileError::None;
    return result;
}

LicenseFileError LicenseCodec::writeFile(const QDate &expiresDate,
                                         const QString &filePath)
{
    if (!expiresDate.isValid()) {
        return LicenseFileError::InvalidDate;
    }

    const QFileInfo fileInfo(filePath);
    const QDir directory = fileInfo.absoluteDir();
    if (!directory.exists()
            && !QDir().mkpath(directory.absolutePath())) {
        return LicenseFileError::FileWriteFailed;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return LicenseFileError::FileWriteFailed;
    }

    QString content;
    content += QStringLiteral("[License]\n");
    content += QStringLiteral("data=%1\n").arg(
                encryptedPayloadForDate(expiresDate));
    file.write(content.toUtf8());
    return file.commit()
            ? LicenseFileError::None
            : LicenseFileError::FileWriteFailed;
}
