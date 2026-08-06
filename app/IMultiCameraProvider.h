#ifndef IMULTICAMERAPROVIDER_H
#define IMULTICAMERAPROVIDER_H

#include "MultiCameraTypes.h"

class IMultiCameraProvider
{
public:
    virtual ~IMultiCameraProvider() {}

    virtual bool openAll() = 0;
    virtual void closeAll() = 0;
    virtual bool startAll() = 0;
    virtual void stopAll() = 0;
    virtual bool triggerOnce() = 0;
    virtual bool grabShot(MultiCameraShot& shot, int timeoutMs) = 0;
    virtual MultiCameraStatus status() const = 0;
};

#endif // IMULTICAMERAPROVIDER_H
