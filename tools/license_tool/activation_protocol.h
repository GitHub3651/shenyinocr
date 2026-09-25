#pragma once

#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>

struct LicenseToolModeDescriptor
{
    const char *modeId;
    const char *displayName;
};

struct ActivationRequestData
{
    QString deviceBinding;
    QString deviceDigest;
};

struct ActivationCodeData
{
    QString deviceBinding;
    QString deviceDigest;
    bool permanent = false;
    QDate expiresDate;
    bool showExpiry = false;
    QStringList featureModeIds;
    QString defaultModeId;
};

struct LicenseFileData
{
    bool permanent = false;
    QDate expiresDate;
    bool showExpiry = false;
    QStringList featureModeIds;
    QString defaultModeId;
    QString deviceBinding;
    QString deviceCode;
};

const QVector<LicenseToolModeDescriptor> &licenseToolModeDescriptors();
bool parseActivationRequestCode(
    const QString &code,
    ActivationRequestData *request);
QString createActivationCode(const ActivationCodeData &activation);
bool parseActivationCode(
    const QString &code,
    ActivationCodeData *activation);
bool readLicenseFile(const QString &filePath,
                     LicenseFileData *license);
