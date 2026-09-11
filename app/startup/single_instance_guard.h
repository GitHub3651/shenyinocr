#pragma once

#include <QSharedMemory>
#include <QString>

class SingleInstanceGuard
{
public:
    explicit SingleInstanceGuard(const QString &key);

    bool acquire();

private:
    QSharedMemory m_sharedMemory;
};
