#pragma once

#include "appsettingsmanager.h"

class MachineSettingsPolicy
{
public:
    static GlobalSettings defaultsForHardwareState(
        const GlobalSettings &applied,
        const GlobalSettings &defaults,
        bool cameraOpen,
        bool plcConnected);
};
