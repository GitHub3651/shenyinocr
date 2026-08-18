// 文件作用：本文件用于集中给出机器设置默认值、范围检查和跨字段约束。
// 主要职责：集中给出机器设置默认值、范围检查和跨字段约束。
// 模块位置：系统支撑层；提供设置、日志、授权和崩溃诊断等基础能力。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "system_support/settings/machine_settings.h"

// 组件说明：MachineSettingsPolicy 组件集中描述相关配置、规则和运行参数。
class MachineSettingsPolicy
{
public:
    static MachineSettings defaultsForHardwareState(
        const MachineSettings &applied,
        const MachineSettings &defaults,
        bool cameraOpen,
        bool plcConnected);
};
