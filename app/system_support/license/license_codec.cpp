#include "system_support/license/license_codec.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
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

QString encryptText(const QString &text)
{
    return QString::fromLatin1(
                cryptData(text.toUtf8()).toHex().toUpper());
}

QString decryptText(const QString &text)
{
    return QString::fromUtf8(cryptData(
                QByteArray::fromHex(text.toLatin1())));
}

bool containsRequiredFields(const QMap<QString, QString> &values)
{
    return values.contains(QStringLiteral("issuedDate"))
            && values.contains(QStringLiteral("expires"))
            && values.contains(QStringLiteral("features"))
            && values.contains(QStringLiteral("defaultMode"))
            && values.contains(QStringLiteral("deviceBinding"))
            && values.contains(QStringLiteral("deviceCode"));
}

void appendEncryptedField(QString &content,
                          const QString &name,
                          const QString &value)
{
    content.append(encryptText(name));
    content.append(QLatin1Char('='));
    content.append(encryptText(value));
    content.append(QLatin1Char('\n'));
}

} // namespace

LicenseDecodeResult LicenseCodec::readFile(const QString &filePath)
{
    LicenseDecodeResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    if (stream.atEnd()
            || stream.readLine() != QStringLiteral("version=4")) {
        result.status = LicenseCodecStatus::InvalidFormat;
        return result;
    }

    QMap<QString, QString> values;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const int separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0) {
            continue;
        }
        values.insert(
                    decryptText(line.left(separator)),
                    decryptText(line.mid(separator + 1)));
    }

    if (!containsRequiredFields(values)) {
        result.status = LicenseCodecStatus::InvalidFormat;
        return result;
    }

    const QString issuedDate = values.value(QStringLiteral("issuedDate"));
    result.license.issuedDate = QDate::fromString(
                issuedDate, QStringLiteral("yyyy-MM-dd"));
    if (!result.license.issuedDate.isValid()
            || result.license.issuedDate.toString(
                QStringLiteral("yyyy-MM-dd")) != issuedDate) {
        result.status = LicenseCodecStatus::InvalidFormat;
        return result;
    }

    const QString expires = values.value(QStringLiteral("expires"));
    if (expires == QLatin1String("permanent")) {
        result.license.permanent = true;
    } else {
        result.license.expiresDate = QDate::fromString(
                    expires, QStringLiteral("yyyy-MM-dd"));
        if (!result.license.expiresDate.isValid()
                || result.license.expiresDate.toString(
                    QStringLiteral("yyyy-MM-dd")) != expires) {
            result.status = LicenseCodecStatus::InvalidFormat;
            return result;
        }
    }

    result.license.featureModeIds = values.value(
                QStringLiteral("features")).split(
                QLatin1Char(','), QString::KeepEmptyParts);
    result.license.defaultModeId = values.value(
                QStringLiteral("defaultMode"));
    result.license.deviceBinding = values.value(
                QStringLiteral("deviceBinding"));
    result.license.deviceCode = values.value(
                QStringLiteral("deviceCode"));
    result.status = LicenseCodecStatus::Success;
    return result;
}

LicenseCodecStatus LicenseCodec::writeFile(
        const LicenseData &license,
        const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    const QDir directory = fileInfo.absoluteDir();
    if (!directory.exists()
            && !QDir().mkpath(directory.absolutePath())) {
        return LicenseCodecStatus::FileWriteFailed;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return LicenseCodecStatus::FileWriteFailed;
    }

    QString content = QStringLiteral("version=4\n");
    appendEncryptedField(content, QStringLiteral("issuedDate"),
                         license.issuedDate.toString(
                             QStringLiteral("yyyy-MM-dd")));
    appendEncryptedField(content, QStringLiteral("expires"),
                         license.permanent
                         ? QStringLiteral("permanent")
                         : license.expiresDate.toString(
                             QStringLiteral("yyyy-MM-dd")));
    appendEncryptedField(content, QStringLiteral("features"),
                         license.featureModeIds.join(QLatin1Char(',')));
    appendEncryptedField(content, QStringLiteral("defaultMode"),
                         license.defaultModeId);
    appendEncryptedField(content, QStringLiteral("deviceBinding"),
                         license.deviceBinding);
    appendEncryptedField(content, QStringLiteral("deviceCode"),
                         license.deviceCode);
    file.write(content.toUtf8());
    return file.commit()
            ? LicenseCodecStatus::Success
            : LicenseCodecStatus::FileWriteFailed;
}
