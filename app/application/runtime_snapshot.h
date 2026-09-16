#pragma once

#include <QMetaType>

enum class ApplicationRuntimeState
{
    Idle,
    Starting,
    Running,
    Stopping
};

struct RuntimeSnapshot
{
    ApplicationRuntimeState state = ApplicationRuntimeState::Idle;
    bool cameraOpen = false;
    bool plcConnected = false;
    bool isInspectionBusy() const
    {
        return state == ApplicationRuntimeState::Starting
                || state == ApplicationRuntimeState::Running
                || state == ApplicationRuntimeState::Stopping;
    }
};

Q_DECLARE_METATYPE(RuntimeSnapshot)
