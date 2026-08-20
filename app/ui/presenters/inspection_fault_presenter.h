// 文件作用：本文件用于把运行时故障快照转换为界面可显示的故障文字。
// 主要职责：把运行时故障快照转换为状态、结果和操作员提示文字。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "application/inspection_ui_contract.h"

#include <QString>

// 组件说明：InspectionFaultPresentation 数据结构集中传递该流程需要的只读数据或回调。
struct InspectionFaultPresentation
{
    QString statusText;
    QString resultText;
    QString operatorMessage;

    bool isValid() const;
};

// 组件说明：InspectionFaultPresenter 组件负责对应界面区域的显示和用户交互。
class InspectionFaultPresenter
{
public:
    static InspectionFaultPresentation create(
        const ApplicationFaultSnapshot &snapshot);
};
