#pragma once

#include <QString>
#include <QStringList>
#include <QMetaType>

enum class ApplicationRuntimeState
{
    Idle,
    Starting,
    Running,
    Stopping,
    Fault
};

struct RuntimeSnapshot
{
    ApplicationRuntimeState state = ApplicationRuntimeState::Idle;
    bool cameraOpen = false;
    bool plcConnected = false;
    QString runId;
    QStringList activeTemplatePaths;
    bool isInspectionBusy() const
    {
        return state == ApplicationRuntimeState::Starting
                || state == ApplicationRuntimeState::Running
                || state == ApplicationRuntimeState::Stopping
                || state == ApplicationRuntimeState::Fault;
    }
};

Q_DECLARE_METATYPE(RuntimeSnapshot)
