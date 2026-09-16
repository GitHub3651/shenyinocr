#pragma once

#include <QString>

class QWidget;

enum class OperationUiState
{
    CameraClosed = 0,
    CameraReady,
    Detecting,
    Stopping,
    TemplatePreviewing,
    TemplateFrozen
};

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
    Access imageSettings;
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

struct OperationUiContext
{
    OperationUiState state = OperationUiState::CameraClosed;
    bool cameraOpen = false;
    bool plcConnected = false;
};

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
