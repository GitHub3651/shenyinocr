#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

enum class LicenseCodecStatus
{
    Success,
    FileReadFailed,
    InvalidFormat,
    FileWriteFailed
};

struct LicenseData
{
    bool permanent = false;
    QDate expiresDate;
    QStringList featureModeIds;
    QString defaultModeId;
    QString deviceBinding;
    QString deviceCode;
};

struct LicenseDecodeResult
{
    LicenseCodecStatus status = LicenseCodecStatus::FileReadFailed;
    LicenseData license;

    bool succeeded() const;
};

class LicenseCodec
{
public:
    static LicenseDecodeResult readFile(const QString &filePath);
    static LicenseCodecStatus writeFile(const LicenseData &license,
                                        const QString &filePath);
};
