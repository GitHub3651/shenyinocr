#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

enum class RuntimeGuardStatus
{
    Valid,
    Expired,
    TimeError,
    DeviceBindingError,
    Invalid
};

struct RuntimeGuardResult
{
    RuntimeGuardStatus status = RuntimeGuardStatus::Invalid;
    bool permanent = false;
    QDate issuedDate;
    QDate expiresDate;
    int remainingDays = -1;
    QStringList authorizedModeIds;
    QString defaultModeId;
};

class RuntimeGuard
{
public:
    static RuntimeGuardResult check();
    static RuntimeGuardStatus checkRuntimeDate(const QDate &issuedDate,
                                               const QDate &expiresDate);
};
