// 文件作用：本文件用于定义应用和界面读取的运行状态只读快照。
// 主要职责：定义应用和界面读取的运行状态只读快照。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、模板和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QString>
#include <QStringList>
#include <QMetaType>

#include "runtime/result_export_client.h"

// 组件说明：ApplicationRuntimeState 枚举列出该组件允许使用的稳定状态和选项。
enum class ApplicationRuntimeState
{
    Idle,
    Starting,
    Running,
    Stopping,
    Fault
};

// 组件说明：RuntimeSnapshot 数据结构集中传递该流程需要的只读数据或回调。
struct RuntimeSnapshot
{
    ApplicationRuntimeState state = ApplicationRuntimeState::Idle;
    bool cameraOpen = false;
    bool plcConnected = false;
    QString runId;
    QStringList activeTemplatePaths;
    ResultExportConnectionState resultExportConnectionState =
            ResultExportConnectionState::Disconnected;
    bool resultExportStartupReady = false;
    bool resultExportDispositionPending = false;
    int resultExportPendingCount = 0;
    double resultExportRoundTripMs = -1.0;
    bool resultExportEnabled = false;

    // 函数说明：isInspectionBusy 函数检查相关状态并返回判断结果。
    bool isInspectionBusy() const
    {
        return state == ApplicationRuntimeState::Starting
                || state == ApplicationRuntimeState::Running
                || state == ApplicationRuntimeState::Stopping
                || state == ApplicationRuntimeState::Fault;
    }
};

Q_DECLARE_METATYPE(RuntimeSnapshot)
