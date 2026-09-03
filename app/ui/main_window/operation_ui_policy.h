// 文件作用：本文件用于根据运行状态计算按钮文字、启用状态和可执行操作。
// 主要职责：根据运行状态计算按钮文字、启用状态和可执行操作。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QString>

class QWidget;

// 组件说明：OperationUiState 枚举列出该组件允许使用的稳定状态和选项。
enum class OperationUiState
{
    CameraClosed = 0,
    CameraReady,
    Detecting,
    Stopping,
    Fault,
    TemplatePreviewing,
    TemplateFrozen
};

// 组件说明：OperationUiSnapshot 数据结构集中传递该流程需要的只读数据或回调。
struct OperationUiSnapshot
{
    struct Access
    {
        bool enabled = false;
        QString disabledReason;
    };

    bool cameraOpen = false;
    bool plcConnected = false;
    Access openCamera;
    Access startDetection;
    Access stop;
    Access closeCamera;
    Access templateCapture;
    Access saveTemplate;
    Access generalSettings;
    Access cameraSettings;
    Access plcConnection;
    Access plcRuntime;
    Access templateSelection;
    Access templateEditing;
    Access statisticsReset;
    Access rejectQueueReset;
    QString startDetectionText;
    QString stopText;
    QString templateCaptureText;
    QString statusText;
};

// 组件说明：OperationUiContext 汇总一次UI权限计算所需的唯一状态快照。
struct OperationUiContext
{
    OperationUiState state = OperationUiState::CameraClosed;
    bool cameraOpen = false;
    bool plcConnected = false;
};

// 组件说明：OperationUiPolicy 组件集中描述相关配置、规则和运行参数。
class OperationUiPolicy
{
public:
    static OperationUiSnapshot create(const OperationUiContext &context);
};

// 将统一权限快照应用到控件；禁用时显示原因，重新启用时恢复控件原提示。
void applyOperationUiAccess(
    QWidget *widget,
    const OperationUiSnapshot::Access &access,
    bool showDisabledReason = true);
