#pragma once

#include "ui/presenters/detection_result_presenter.h"
#include "runtime/inspection_fault_state.h"
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
        QWidget *multiCameraWidget,
        QTimer *templateAttentionTimer,
        bool *templateAttentionOn,
        const Callbacks &callbacks);

    DetectionResultViewBindings resultViewBindings() const;
    void setMultiCameraWidget(QWidget *multiCameraWidget);
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
    QWidget *m_multiCameraWidget = nullptr;
    QTimer *m_templateAttentionTimer = nullptr;
    bool *m_templateAttentionOn = nullptr;
    Callbacks m_callbacks;
    bool m_detectionRoiWarningActive = false;
    quint64 m_imageSaveFailedCount = 0;
    QString m_latestImageSaveError;
    bool m_imageSaveWarningScheduled = false;
};
