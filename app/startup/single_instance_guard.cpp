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

    // Marker creation failure does not prove another instance is running.
    m_sharedMemory.create(1);
    return true;
}
