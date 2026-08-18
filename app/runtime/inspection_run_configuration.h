// 文件作用：本文件用于定义一次检测运行使用的触发计划、定位配置和保存策略。
// 主要职责：定义一次检测运行使用的触发计划、定位配置和保存策略。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/positioning/inspection_positioner.h"

enum class InspectionAcquisitionKind {
    SoftwareTrigger,
    HardwareTrigger
};

// 组件说明：InspectionRunPlan 组件集中描述相关配置、规则和运行参数。
struct InspectionRunPlan
{
    InspectionAcquisitionKind acquisitionKind =
            InspectionAcquisitionKind::SoftwareTrigger;
    InspectionTrackingKind trackingKind =
            InspectionTrackingKind::SingleTemplate;
    bool barcodeWordHardTriggerMode = false;
};

// 组件说明：InspectionRunConfiguration 组件集中描述相关配置、规则和运行参数。
class InspectionRunConfiguration
{
public:
    static InspectionRunPlan createPlan(
        InspectionTrackingKind trackingKind,
        bool hardwareTriggerEnabled,
        bool barcodeWordMode = false);
};
