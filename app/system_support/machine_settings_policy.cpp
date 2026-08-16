#include "system_support/machine_settings_policy.h"

MachineSettings MachineSettingsPolicy::defaultsForHardwareState(
    const MachineSettings &applied,
    const MachineSettings &defaults,
    bool cameraOpen,
    bool plcConnected)
{
    MachineSettings editable = applied;
    editable.detectModeId = defaults.detectModeId;
    editable.imageSaveModeId = defaults.imageSaveModeId;
    editable.imageSaveTypeId = defaults.imageSaveTypeId;
    editable.imageSavePath = defaults.imageSavePath;
    editable.colorChannelId = defaults.colorChannelId;
    editable.imageRotationId = defaults.imageRotationId;
    editable.triggerEnabled = defaults.triggerEnabled;
    editable.publishedRecipeIdsByMode.clear();
    if (cameraOpen) {
        editable.cameraExposure = defaults.cameraExposure;
        editable.cameraGain = defaults.cameraGain;
    }
    if (!plcConnected) {
        editable.plcIp = defaults.plcIp;
        editable.plcRack = defaults.plcRack;
        editable.plcSlot = defaults.plcSlot;
    } else {
        editable.triggerModeId = defaults.triggerModeId;
        editable.photoDistance = defaults.photoDistance;
        editable.photoTime = defaults.photoTime;
        editable.cameraDelay = defaults.cameraDelay;
        editable.rejectDistance = defaults.rejectDistance;
        editable.rejectTime = defaults.rejectTime;
        editable.rejectPosition = defaults.rejectPosition;
    }
    return editable;
}
