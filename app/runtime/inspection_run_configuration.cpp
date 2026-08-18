// 文件作用：本文件用于定义一次检测运行使用的触发计划、定位配置和保存策略。
// 主要职责：定义一次检测运行使用的触发计划、定位配置和保存策略。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/inspection_run_configuration.h"

// 函数说明：createPlan 函数创建、准备或启动对应流程。
InspectionRunPlan InspectionRunConfiguration::createPlan(
    bool hardwareTriggerEnabled)
{
    InspectionRunPlan plan;
    plan.acquisitionKind = hardwareTriggerEnabled
            ? InspectionAcquisitionKind::HardwareTrigger
            : InspectionAcquisitionKind::SoftwareTrigger;
    return plan;
}
