#pragma once

#include "contracts/inspection_presentation.h"
#include "runtime/inspection_runtime.h"
#include "ui/main_window/operation_ui_policy.h"

#include <QString>
#include <QTimer>

class QWidget;

namespace Ui {
class MainWindow;
class InspectionInfoPage;
}

enum class InspectionClearScope
{
    ImageMetadata,
    AllDetectionData
};

class InspectionPage
{
public:
    InspectionPage(
        QWidget &rootWidget,
        Ui::MainWindow &mainWindowUi,
        Ui::InspectionInfoPage &inspectionInfoUi);
    InspectionPage(const InspectionPage &) = delete;
    InspectionPage &operator=(const InspectionPage &) = delete;

    void present(const InspectionPresentation &presentation);
    void setStatistics(const DetectionResultStatistics &statistics);
    void presentPreviewImage(const QImage &image);
    void clearInspectionView(InspectionClearScope scope);
    void clearTransientView();
    void applyOperationState(
        OperationUiState requestedState,
        const OperationUiSnapshot &operationUi);
    void presentFault(
        const InspectionFaultSnapshot &snapshot,
        bool &alarmPresented);
    bool confirmFaultRecovery(
        const InspectionFaultSnapshot &snapshot) const;
    void restoreNormalFaultStyle();
    void reportImageSaveFailure(
        quint64 totalFailed,
        const QString &latestError);

private:
    QWidget &m_rootWidget;
    Ui::MainWindow &m_mainWindowUi;
    Ui::InspectionInfoPage &m_inspectionInfoUi;
    QTimer m_templateAttentionTimer;
    bool m_templateAttentionOn = false;
    quint64 m_imageSaveFailedCount = 0;
    bool m_imageSaveWarningScheduled = false;
};
