#pragma once

#include "system_support/settings/machine_settings.h"

class MachineSettingsPolicy
{
public:
    static MachineSettings defaultsForHardwareState(
        const MachineSettings &applied,
        const MachineSettings &defaults,
        bool cameraOpen,
        bool plcConnected);
};
