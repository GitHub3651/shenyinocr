#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

struct RuntimeGuardResult
{
    bool valid = false;
    bool permanent = false;
    QDate expiresDate;
    QStringList authorizedModeIds;
    QString defaultModeId;

    bool succeeded() const;
};

class RuntimeGuard
{
public:
    static RuntimeGuardResult check();
};
