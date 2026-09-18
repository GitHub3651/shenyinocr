#include "ui/main_window/inspection/inspection_page.h"

#include "ui/main_window/inspection_image_canvas.h"
#include "ui/main_window/verdict_result_label.h"
#include "ui_inspection_info_page.h"
#include "ui_main_window.h"

#include <QLabel>
#include <QLineEdit>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QVariant>
#include <QWidget>

namespace {

void setLabelTextIfChanged(QLabel &label, const QString &text)
{
    if (label.text() != text) {
        label.setText(text);
    }
}

QString breakableRecognitionText(const QString &text)
{
    QString displayText;
    displayText.reserve(text.size() * 2);
    for (const QChar character : text) {
        displayText.append(character);
        if (!character.isSpace()) {
            displayText.append(QChar(0x200B));
        }
    }
    return displayText;
}

void setStyleProperty(
    QWidget &widget,
    const char *name,
    const QVariant &value)
{
    if (widget.property(name) == value) {
        return;
    }
    widget.setProperty(name, value);
    widget.style()->unpolish(&widget);
    widget.style()->polish(&widget);
    widget.update();
}

} // namespace

InspectionPage::InspectionPage(
    Ui::MainWindow &mainWindowUi,
    Ui::InspectionInfoPage &inspectionInfoUi)
    : m_mainWindowUi(mainWindowUi),
      m_inspectionInfoUi(inspectionInfoUi)
{
}

void InspectionPage::present(const InspectionPresentation &presentation)
{
    if (!presentation.isValid()) {
        return;
    }
    const QPixmap pixmap = QPixmap::fromImage(presentation.image);
    m_mainWindowUi.inspectionImageCanvas->setAutoFitPixmap(pixmap);
    m_mainWindowUi.label_verdictResult->showVerdict(
                presentation.verdictStyle);
    setLabelTextIfChanged(
                *m_inspectionInfoUi.label_recognitionText,
                breakableRecognitionText(
                    presentation.recognitionText));
    m_inspectionInfoUi.lineEdit_currentTemplateName->setText(
                presentation.updatesTemplateName
                ? presentation.templateName : QString());
    setStatistics(presentation.statistics);
    m_inspectionInfoUi.lineEdit_detectionDuration->setText(
                presentation.elapsedText);
}

void InspectionPage::setStatistics(
    const DetectionResultStatistics &statistics)
{
    m_inspectionInfoUi.lineEdit_totalCount->setText(
                QString::number(statistics.totalCount));
    m_inspectionInfoUi.lineEdit_ngCount->setText(
                QString::number(statistics.ngCount));
    m_inspectionInfoUi.lineEdit_passRate->setText(
                QString::number(statistics.passRatePercent(), 'f', 1));
}

void InspectionPage::presentPreviewImage(const QImage &image)
{
    if (image.isNull()) {
        return;
    }
    clearInspectionView(InspectionClearScope::ImageMetadata);
    m_mainWindowUi.inspectionImageCanvas->setAutoFitPixmap(QPixmap::fromImage(image));
}

void InspectionPage::clearInspectionView(InspectionClearScope scope)
{
    m_mainWindowUi.label_verdictResult->clearVerdict();
    setLabelTextIfChanged(*m_inspectionInfoUi.label_recognitionText, QString());
    m_inspectionInfoUi.lineEdit_detectionDuration->clear();
    m_inspectionInfoUi.lineEdit_currentTemplateName->clear();
    if (scope == InspectionClearScope::AllDetectionData) {
        m_mainWindowUi.inspectionImageCanvas->clear();
        m_inspectionInfoUi.lineEdit_totalCount->clear();
        m_inspectionInfoUi.lineEdit_ngCount->clear();
        m_inspectionInfoUi.lineEdit_passRate->clear();
    }
}

void InspectionPage::applyOperationState(
    OperationUiState requestedState,
    const OperationUiSnapshot &operationUi)
{
    m_mainWindowUi.toolButton_cameraAction->setText(
                operationUi.cameraActionText);
    m_mainWindowUi.toolButton_cameraAction->setIcon(QIcon(
                operationUi.cameraOpen
                ? QStringLiteral(":/svg/camera_off.svg")
                : QStringLiteral(":/svg/camera_on.svg")));
    m_mainWindowUi.toolButton_inspectionAction->setText(
                operationUi.inspectionActionText);
    m_mainWindowUi.toolButton_inspectionAction->setIcon(QIcon(
                requestedState == OperationUiState::Detecting
                || requestedState == OperationUiState::Stopping
                ? QStringLiteral(":/svg/stop.svg")
                : QStringLiteral(":/svg/start.svg")));
    applyOperationUiAccess(
                m_mainWindowUi.toolButton_cameraAction,
                operationUi.cameraAction);
    applyOperationUiAccess(
                m_mainWindowUi.toolButton_inspectionAction,
                operationUi.inspectionAction);
    m_mainWindowUi.toolButton_previewAction->setText(
                operationUi.previewActionText);
    m_mainWindowUi.toolButton_previewAction->setIcon(QIcon(
                requestedState == OperationUiState::CameraPreviewing
                ? QStringLiteral(":/svg/stop.svg")
                : QStringLiteral(":/svg/preview.svg")));
    applyOperationUiAccess(
                m_mainWindowUi.toolButton_previewAction,
                operationUi.previewAction);
    m_inspectionInfoUi.label_runtimeStatus->setText(operationUi.statusText);
    setStyleProperty(
                *m_inspectionInfoUi.label_runtimeStatus,
                "uiState",
                operationUi.statusUiState);
}
