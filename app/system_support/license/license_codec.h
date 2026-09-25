#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

enum class LicenseCodecStatus
{
    Success,
    FileMissing,
    FileReadFailed,
    InvalidFormat
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

struct LicenseData
{
    bool permanent = false;
    QDate expiresDate;
    bool showExpiry = false;
    QStringList featureModeIds;
    QString defaultModeId;
    QString deviceBinding;
    QString deviceCode;
};

struct LicenseDecodeResult
{
    LicenseCodecStatus status = LicenseCodecStatus::FileReadFailed;
    LicenseData license;
};

class LicenseCodec
{
public:
    static QString deviceDigest(const QString &deviceBinding,
                                const QString &deviceCode);
    static QString createActivationRequestCode(
        const QString &deviceBinding,
        const QString &deviceCode);
    static bool parseActivationCode(
        const QString &code,
        ActivationCodeData *activation);

    static LicenseDecodeResult readFile(const QString &filePath);
    static bool writeFile(const LicenseData &license,
                          const QString &filePath);
};
