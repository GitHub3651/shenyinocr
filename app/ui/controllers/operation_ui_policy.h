// 文件作用：本文件用于根据运行状态计算按钮文字、启用状态和可执行操作。
// 主要职责：根据运行状态计算按钮文字、启用状态和可执行操作。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QString>

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
    bool enableAllOperations = false;
    bool openCameraEnabled = false;
    bool startDetectionEnabled = false;
    bool stopEnabled = false;
    bool closeCameraEnabled = false;
    bool templateCaptureEnabled = false;
    bool saveTemplateEnabled = false;
    bool settingsEnabled = false;
    QString startDetectionText;
    QString stopText;
    QString templateCaptureText;
    QString statusText;
};

// 组件说明：OperationUiPolicy 组件集中描述相关配置、规则和运行参数。
class OperationUiPolicy
{
public:
    static OperationUiSnapshot create(OperationUiState state);
};
