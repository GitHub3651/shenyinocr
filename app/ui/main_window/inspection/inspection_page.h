#pragma once

#include "contracts/inspection_presentation.h"
#include "ui/main_window/operation_ui_policy.h"

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

private:
    Ui::MainWindow &m_mainWindowUi;
    Ui::InspectionInfoPage &m_inspectionInfoUi;
};
