#pragma once

#include <QString>

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

class OperationUiPolicy
{
public:
    static OperationUiSnapshot create(OperationUiState state);
};
