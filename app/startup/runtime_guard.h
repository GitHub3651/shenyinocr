#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

enum class RuntimeGuardStatus
{
    Valid,
    LicenseMissing,
    LicenseInvalid,
    LicenseExpired,
    DeviceMismatch,
    LicenseReadFailed,
    DeviceUnavailable,
    LicenseSaveFailed
};

struct RuntimeGuardResult
{
    RuntimeGuardStatus status = RuntimeGuardStatus::LicenseInvalid;
    bool permanent = false;
    QDate expiresDate;
    bool showExpiry = false;
    QStringList authorizedModeIds;
    QString defaultModeId;
    QString activationRequestCode;
};

class RuntimeGuard
{
public:
    static RuntimeGuardResult check();
    static RuntimeGuardResult activate(const QString &activationCode);
};
