#pragma once

#include "application/inspection_ui_contract.h"
#include "ui/controllers/operation_ui_policy.h"

#include <QString>

#include <functional>

class QTimer;
class QWidget;

namespace Ui {
class MainWindow;
}

class InspectionPage
{
public:
    struct Callbacks
    {
        std::function<void(bool)> setSettingsEnabled;
        std::function<void()> refreshHardwareSettingsEnabled;
        std::function<void(const QString &)> updateImageDisplayStatus;
    };

    InspectionPage(
        QWidget *rootWidget,
        Ui::MainWindow *ui,
        QTimer *templateAttentionTimer,
        bool *templateAttentionOn,
        const Callbacks &callbacks);
    InspectionPage(const InspectionPage &) = delete;
    InspectionPage &operator=(const InspectionPage &) = delete;

    InspectionViewBindingsDto resultViewBindings() const;
    void updateOperationState(
        OperationUiState requestedState,
        bool runtimeFaulted);
    void presentFault(
        const ApplicationFaultSnapshot &snapshot,
        bool *alarmPresented);
    bool confirmFaultRecovery(
        const ApplicationFaultSnapshot &snapshot) const;
    void restoreNormalFaultStyle();
    void showDetectionRoiWarning();
    void clearDetectionRoiWarning(const QString &runningStatusText);
    void warnMissingAnnotatedImage() const;
    void reportImageSaveFailure(
        quint64 totalFailed,
        const QString &latestError);

private:
    QWidget *m_rootWidget = nullptr;
    Ui::MainWindow *m_ui = nullptr;
    QTimer *m_templateAttentionTimer = nullptr;
    bool *m_templateAttentionOn = nullptr;
    Callbacks m_callbacks;
    bool m_detectionRoiWarningActive = false;
    quint64 m_imageSaveFailedCount = 0;
    QString m_latestImageSaveError;
    bool m_imageSaveWarningScheduled = false;
};
