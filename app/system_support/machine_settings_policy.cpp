#include "system_support/machine_settings_policy.h"

GlobalSettings MachineSettingsPolicy::defaultsForHardwareState(
    const GlobalSettings &applied,
    const GlobalSettings &defaults,
    bool cameraOpen,
    bool plcConnected)
{
    GlobalSettings editable = applied;
    editable.detectModeId = defaults.detectModeId;
    editable.imageSaveModeId = defaults.imageSaveModeId;
    editable.imageSaveTypeId = defaults.imageSaveTypeId;
    editable.imageSavePath = defaults.imageSavePath;
    editable.templateBaseDirPath = defaults.templateBaseDirPath;
    editable.colorChannelId = defaults.colorChannelId;
    editable.imageRotationId = defaults.imageRotationId;
    editable.triggerEnabled = defaults.triggerEnabled;
    editable.tissueRoughnessThreshold = defaults.tissueRoughnessThreshold;
    editable.templateDirPathsByMode.clear();
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
        editable.plcModeId = defaults.plcModeId;
        editable.photoDistance = defaults.photoDistance;
        editable.photoTime = defaults.photoTime;
        editable.cameraDelay = defaults.cameraDelay;
        editable.rejectDistance = defaults.rejectDistance;
        editable.rejectTime = defaults.rejectTime;
        editable.rejectPosition = defaults.rejectPosition;
    }
    return editable;
}
