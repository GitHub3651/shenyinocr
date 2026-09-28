#include "startup/single_instance_guard.h"

SingleInstanceGuard::SingleInstanceGuard(const QString &key)
    : m_sharedMemory(key)
{
}

bool SingleInstanceGuard::acquire()
{
    return m_sharedMemory.create(1);
}
