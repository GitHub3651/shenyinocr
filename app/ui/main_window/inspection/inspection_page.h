#pragma once

#include "contracts/inspection_presentation.h"
#include "ui/main_window/operation_ui_policy.h"

#include <QString>

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
    void applyOperationState(
        OperationUiState requestedState,
        const OperationUiSnapshot &operationUi);
    void reportImageSaveFailure(
        quint64 totalFailed,
        const QString &latestError);

private:
    QWidget &m_rootWidget;
    Ui::MainWindow &m_mainWindowUi;
    Ui::InspectionInfoPage &m_inspectionInfoUi;
    quint64 m_imageSaveFailedCount = 0;
    bool m_imageSaveWarningScheduled = false;
};
