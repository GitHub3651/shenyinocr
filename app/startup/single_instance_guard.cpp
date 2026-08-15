#include "startup/single_instance_guard.h"

SingleInstanceGuard::SingleInstanceGuard(const QString &key)
    : m_sharedMemory(key)
{
}

bool SingleInstanceGuard::acquire()
{
    if (m_sharedMemory.attach()) {
        return false;
    }

    // Preserve the legacy behavior: inability to create the marker does not
    // block the first process from starting.
    m_sharedMemory.create(1);
    return true;
}
