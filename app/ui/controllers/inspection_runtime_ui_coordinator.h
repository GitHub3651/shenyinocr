#pragma once

#include "runtime/inspection_presentation_renderer.h"
#include "runtime/inspection_runtime.h"
#include "ui/controllers/operation_ui_policy.h"

#include <QString>

#include <functional>

class QTimer;
class QWidget;

namespace Ui {
class Widget;
}

class InspectionRuntimeUiCoordinator
{
public:
    struct Callbacks
    {
        std::function<void(bool)> setSettingsEnabled;
        std::function<void()> refreshHardwareSettingsEnabled;
        std::function<void(const QString &)> updateImageDisplayStatus;
    };

    InspectionRuntimeUiCoordinator(
        QWidget *rootWidget,
        Ui::Widget *ui,
        QTimer *templateAttentionTimer,
        bool *templateAttentionOn,
        const Callbacks &callbacks);

    InspectionPresentationViewBindings resultViewBindings() const;
    void updateOperationState(
        OperationUiState requestedState,
        bool runtimeFaulted);
    void presentFault(
        const InspectionFaultSnapshot &snapshot,
        bool *alarmPresented);
    bool confirmFaultRecovery(
        const InspectionFaultSnapshot &snapshot) const;
    void restoreNormalFaultStyle();
    void showDetectionRoiWarning();
    void clearDetectionRoiWarning(const QString &runningStatusText);
    void warnMissingAnnotatedImage() const;
    void reportImageSaveFailure(
        quint64 totalFailed,
        const QString &latestError);

private:
    QWidget *m_rootWidget = nullptr;
    Ui::Widget *m_ui = nullptr;
    QTimer *m_templateAttentionTimer = nullptr;
    bool *m_templateAttentionOn = nullptr;
    Callbacks m_callbacks;
    bool m_detectionRoiWarningActive = false;
    quint64 m_imageSaveFailedCount = 0;
    QString m_latestImageSaveError;
    bool m_imageSaveWarningScheduled = false;
};
