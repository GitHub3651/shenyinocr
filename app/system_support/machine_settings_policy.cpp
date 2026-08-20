// 文件作用：本文件用于集中给出机器设置默认值、范围检查和跨字段约束。
// 主要职责：集中给出机器设置默认值、范围检查和跨字段约束。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "system_support/machine_settings_policy.h"

// 函数说明：defaultsForHardwareState 函数实现名称所表示的处理步骤。
AppSettings MachineSettingsPolicy::defaultsForHardwareState(
    const AppSettings &applied,
    const AppSettings &defaults,
    bool cameraOpen,
    bool plcConnected)
{
    AppSettings editable = applied;
    editable.detectModeId = defaults.detectModeId;
    editable.imageSaveModeId = defaults.imageSaveModeId;
    editable.imageSaveTypeId = defaults.imageSaveTypeId;
    editable.imageSavePath = defaults.imageSavePath;
    editable.colorChannelId = defaults.colorChannelId;
    editable.imageRotationId = defaults.imageRotationId;
    editable.triggerEnabled = defaults.triggerEnabled;
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
