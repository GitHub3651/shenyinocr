#include "RuntimeGuard.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QMessageAuthenticationCode>
#include <QSaveFile>
#include <QSettings>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

const char kCacheDirName[] = "cache";
const char kLicenseFileName[] = "syscache.dat";
const char kRequestFileName[] = "syscache.req";
const char kStateFileName[] = "sysstate.dat";
const char kVersion[] = "1";

QByteArray secretKey()
{
    return QByteArrayLiteral("AutoOCRproject.syscache.v1.20260526");
}

QString cacheDirPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QLatin1String(kCacheDirName));
}

QString fileInCacheDir(const QString &fileName)
{
    return QDir(cacheDirPath()).filePath(fileName);
}

bool ensureCacheDir()
{
    QDir dir(cacheDirPath());
    if (dir.exists()) {
        return true;
    }

    return dir.mkpath(QStringLiteral("."));
}

QString readMachineGuid()
{
#ifdef Q_OS_WIN
    QSettings settings(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography"),
                       QSettings::NativeFormat);
    return settings.value(QStringLiteral("MachineGuid")).toString().trimmed().toLower();
#else
    return QString();
#endif
}

QString readSystemVolumeSerial()
{
#ifdef Q_OS_WIN
    QString rootPath = qEnvironmentVariable("SystemDrive");
    if (rootPath.isEmpty()) {
        rootPath = QDir::rootPath();
    }
    if (!rootPath.endsWith(QLatin1Char('\\')) && !rootPath.endsWith(QLatin1Char('/'))) {
        rootPath += QLatin1Char('\\');
    }

    DWORD serialNumber = 0;
    const BOOL ok = GetVolumeInformationW(
                reinterpret_cast<LPCWSTR>(rootPath.utf16()),
                nullptr,
                0,
                &serialNumber,
                nullptr,
                nullptr,
                nullptr,
                0);
    if (!ok || serialNumber == 0) {
        return QString();
    }

    return QString::number(serialNumber, 16).rightJustified(8, QLatin1Char('0')).toLower();
#else
    return QString();
#endif
}

QString sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString currentMachineHash()
{
    const QString machineGuid = readMachineGuid();
    const QString volumeSerial = readSystemVolumeSerial();
    if (machineGuid.isEmpty() || volumeSerial.isEmpty()) {
        return QString();
    }

    const QByteArray fingerprint = machineGuid.toUtf8() + "|" + volumeSerial.toUtf8();
    return sha256Hex(fingerprint);
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
    if (!ensureCacheDir()) {
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

bool secureEquals(const QByteArray &left, const QByteArray &right)
{
    if (left.size() != right.size()) {
        return false;
    }

    uchar diff = 0;
    for (int i = 0; i < left.size(); ++i) {
        diff |= static_cast<uchar>(left.at(i) ^ right.at(i));
    }

    return diff == 0;
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

QByteArray statePayload(const QString &lastSeenUtc)
{
    QByteArray payload;
    payload += "version=";
    payload += kVersion;
    payload += "\n";
    payload += "last_seen_utc=";
    payload += lastSeenUtc.toUtf8();
    payload += "\n";
    return payload;
}

bool writeRequestFile(const QString &machineHash)
{
    const QString createdUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QString content;
    content += QStringLiteral("version=%1\n").arg(QLatin1String(kVersion));
    content += QStringLiteral("machine_hash=%1\n").arg(machineHash);
    content += QStringLiteral("created_utc=%1\n").arg(createdUtc);
    return writeTextFile(fileInCacheDir(QLatin1String(kRequestFileName)), content);
}

bool verifyLicenseFile(const QString &machineHash)
{
    const QMap<QString, QString> values = readKeyValueFile(fileInCacheDir(QLatin1String(kLicenseFileName)));
    const QString version = values.value(QStringLiteral("version"));
    const QString fileMachineHash = values.value(QStringLiteral("machine_hash"));
    const QString expires = values.value(QStringLiteral("expires"));
    const QString signature = values.value(QStringLiteral("signature"));

    if (version != QLatin1String(kVersion) || fileMachineHash.isEmpty()
            || expires.isEmpty() || signature.isEmpty()) {
        return false;
    }

    const QByteArray payload = licensePayload(fileMachineHash, expires);
    if (!secureEquals(signature.toLatin1(), hmacSha256Hex(payload))) {
        return false;
    }

    if (fileMachineHash != machineHash) {
        return false;
    }

    const QDate parsedDate = QDate::fromString(expires, QStringLiteral("yyyy-MM-dd"));
    if (!parsedDate.isValid()) {
        return false;
    }

    if (QDate::currentDate() > parsedDate) {
        return false;
    }

    return true;
}

bool updateStateFile()
{
    const QString statePath = fileInCacheDir(QLatin1String(kStateFileName));
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QMap<QString, QString> values = readKeyValueFile(statePath);

    if (!values.isEmpty()) {
        const QString version = values.value(QStringLiteral("version"));
        const QString lastSeenText = values.value(QStringLiteral("last_seen_utc"));
        const QString signature = values.value(QStringLiteral("signature"));

        if (version != QLatin1String(kVersion) || lastSeenText.isEmpty() || signature.isEmpty()) {
            return false;
        }

        const QByteArray payload = statePayload(lastSeenText);
        if (!secureEquals(signature.toLatin1(), hmacSha256Hex(payload))) {
            return false;
        }

        const QDateTime lastSeen = QDateTime::fromString(lastSeenText, Qt::ISODate);
        if (!lastSeen.isValid()) {
            return false;
        }

        if (now.addSecs(300) < lastSeen) {
            return false;
        }
    }

    const QString nowText = now.toString(Qt::ISODate);
    const QByteArray payload = statePayload(nowText);
    QString content;
    content += QString::fromUtf8(payload);
    content += QStringLiteral("signature=%1\n").arg(QString::fromLatin1(hmacSha256Hex(payload)));
    return writeTextFile(statePath, content);
}

} // namespace

bool RuntimeGuard::check()
{
    const QString machineHash = currentMachineHash();
    if (machineHash.isEmpty()) {
        writeRequestFile(machineHash);
        return false;
    }

    const bool ok = verifyLicenseFile(machineHash) && updateStateFile();
    if (!ok) {
        writeRequestFile(machineHash);
        return false;
    }

    return true;
}
