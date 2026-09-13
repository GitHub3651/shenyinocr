#include "ui/main_window/inspection/inspection_page.h"

#include "ui/main_window/inspection/inspection_fault_presenter.h"
#include "ui/main_window/inspection_image_canvas.h"
#include "system_support/logging/log_categories.h"
#include "ui_inspection_info_page.h"
#include "ui_main_window.h"

#include <QAbstractButton>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
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

QString runtimeUiState(OperationUiState state)
{
    switch (state) {
    case OperationUiState::CameraClosed:
        return QStringLiteral("idle");
    case OperationUiState::CameraReady:
        return QStringLiteral("ready");
    case OperationUiState::Detecting:
        return QStringLiteral("running");
    case OperationUiState::Stopping:
        return QStringLiteral("stopping");
    case OperationUiState::Fault:
        return QStringLiteral("fault");
    case OperationUiState::TemplatePreviewing:
    case OperationUiState::TemplateFrozen:
        return QStringLiteral("warning");
    }
    return QStringLiteral("idle");
}

} // namespace

InspectionPage::InspectionPage(
    QWidget &rootWidget,
    Ui::MainWindow &mainWindowUi,
    Ui::InspectionInfoPage &inspectionInfoUi)
    : m_rootWidget(rootWidget),
      m_mainWindowUi(mainWindowUi),
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
    if (!m_mainWindowUi.inspectionImageCanvas->isTemplateDrawingEnabled()) {
        setLabelTextIfChanged(
                    *m_inspectionInfoUi.label_runtimeStatus,
                    QStringLiteral("正在显示相机采集图像..."));
    }
    setStyleProperty(
                *m_mainWindowUi.label_verdictResult,
                "verdict",
                presentation.verdictStyle
                == DetectionVerdictViewStyle::Correct
                ? QStringLiteral("ok")
                : QStringLiteral("ng"));
    setLabelTextIfChanged(
                *m_mainWindowUi.label_verdictResult,
                presentation.verdictText.isEmpty()
                ? (presentation.verdictStyle
                   == DetectionVerdictViewStyle::Correct
                   ? QStringLiteral("正确")
                   : QStringLiteral("错误"))
                : presentation.verdictText);
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
    setLabelTextIfChanged(
                *m_mainWindowUi.label_verdictResult,
                QString());
    setLabelTextIfChanged(*m_inspectionInfoUi.label_recognitionText, QString());
    m_inspectionInfoUi.lineEdit_detectionDuration->clear();
    m_inspectionInfoUi.lineEdit_currentTemplateName->clear();
    setStyleProperty(*m_mainWindowUi.label_verdictResult, "verdict", QStringLiteral("idle"));

    if (scope == InspectionClearScope::AllDetectionData) {
        m_mainWindowUi.inspectionImageCanvas->clear();
        m_inspectionInfoUi.lineEdit_totalCount->clear();
        m_inspectionInfoUi.lineEdit_ngCount->clear();
        m_inspectionInfoUi.lineEdit_passRate->clear();
    }
}

void InspectionPage::clearTransientView()
{
    m_imageSaveWarningScheduled = false;
    m_inspectionInfoUi.label_runtimeStatus->clear();
}

void InspectionPage::applyOperationState(
    OperationUiState requestedState,
    const OperationUiSnapshot &operationUi)
{
    m_mainWindowUi.toolButton_startInspection->setText(operationUi.startDetectionText);
    m_mainWindowUi.toolButton_stopInspection->setText(operationUi.stopText);
    m_mainWindowUi.toolButton_createTemplate->setText(operationUi.templateCaptureText);
    applyOperationUiAccess(m_mainWindowUi.toolButton_openCamera, operationUi.openCamera);
    applyOperationUiAccess(
                m_mainWindowUi.toolButton_startInspection,
                operationUi.startDetection);
    applyOperationUiAccess(m_mainWindowUi.toolButton_stopInspection, operationUi.stop);
    applyOperationUiAccess(m_mainWindowUi.toolButton_closeCamera, operationUi.closeCamera);
    applyOperationUiAccess(
                m_mainWindowUi.toolButton_createTemplate,
                operationUi.templateCapture);
    if (!operationUi.statusText.isEmpty()) {
        m_inspectionInfoUi.label_runtimeStatus->setText(operationUi.statusText);
    }
    const bool keepDetectionWarning =
            requestedState == OperationUiState::Detecting
            && m_inspectionInfoUi.label_runtimeStatus->property("uiState").toString()
               == QStringLiteral("warning")
            && operationUi.statusText.isEmpty();
    if (!keepDetectionWarning) {
        setStyleProperty(
                    *m_inspectionInfoUi.label_runtimeStatus,
                    "uiState",
                    runtimeUiState(requestedState));
    }
}

void InspectionPage::presentFault(
    const InspectionFaultSnapshot &snapshot,
    bool &alarmPresented)
{
    const InspectionFaultPresentation presentation =
            InspectionFaultPresenter::create(snapshot);
    if (!presentation.isValid()) {
        return;
    }
    m_inspectionInfoUi.label_runtimeStatus->setText(presentation.statusText);
    setStyleProperty(
                *m_inspectionInfoUi.label_runtimeStatus,
                "uiState",
                QStringLiteral("fault"));
    m_mainWindowUi.label_verdictResult->setText(presentation.resultText);
    setStyleProperty(
                *m_mainWindowUi.label_verdictResult,
                "verdict",
                QStringLiteral("fault"));

    if (!alarmPresented) {
        alarmPresented = true;
        QMessageBox::critical(
                    &m_rootWidget,
                    QStringLiteral("系统故障－检测已暂停"),
                    presentation.operatorMessage);
    }
}

bool InspectionPage::confirmFaultRecovery(
    const InspectionFaultSnapshot &snapshot) const
{
    const InspectionFaultPresentation presentation =
            InspectionFaultPresenter::create(snapshot);
    if (!presentation.isValid()) {
        return false;
    }

    QMessageBox messageBox(
                QMessageBox::Critical,
                QStringLiteral("故障恢复确认"),
                presentation.operatorMessage
                + QStringLiteral(
                    "\n\n注意：恢复检测操作不代表输送线已停止。"),
                QMessageBox::Yes | QMessageBox::Cancel,
                &m_rootWidget);
    messageBox.setDefaultButton(QMessageBox::Cancel);
    messageBox.button(QMessageBox::Yes)->setText(
                QStringLiteral("确认现场已处理并恢复"));
    messageBox.button(QMessageBox::Cancel)->setText(
                QStringLiteral("继续保持故障锁定"));
    return messageBox.exec() == QMessageBox::Yes;
}

void InspectionPage::restoreNormalFaultStyle()
{
    setStyleProperty(
                *m_inspectionInfoUi.label_runtimeStatus,
                "uiState",
                QStringLiteral("ready"));
    setStyleProperty(
                *m_mainWindowUi.label_verdictResult,
                "verdict",
                QStringLiteral("idle"));
    m_mainWindowUi.label_verdictResult->clear();
}

void InspectionPage::reportImageSaveFailure(
    quint64 totalFailed,
    const QString &)
{
    m_imageSaveFailedCount = totalFailed;
    if (m_imageSaveWarningScheduled) {
        return;
    }
    m_imageSaveWarningScheduled = true;
    QTimer::singleShot(250, &m_rootWidget, [this]() {
        m_imageSaveWarningScheduled = false;
        QString warningText = m_imageSaveFailedCount == 0
                ? QString::fromWCharArray(L"保存失败：未采集到标注图像。")
                : QString::fromWCharArray(
                    L"保存图像失败：累计 %1 个任务。"
                    L"请检查保存文件夹、权限和磁盘空间。")
                .arg(m_imageSaveFailedCount);
        m_inspectionInfoUi.label_runtimeStatus->setWordWrap(true);
        m_inspectionInfoUi.label_runtimeStatus->setText(warningText);
        setStyleProperty(
                    *m_inspectionInfoUi.label_runtimeStatus,
                    "uiState",
                    QStringLiteral("warning"));
    });
}
