#include "ui/controllers/inspection_stop_controller.h"

#include "widget.h"
#include "ui_widget.h"

#include "runtime/inspection_acquisition_controller.h"
#include "runtime/inspection_runtime_stop_transaction.h"
#include "ui/controllers/inspection_result_coordinator.h"
#include "ui/controllers/inspection_runtime_ui_coordinator.h"
#include "ui/controllers/machine_settings_page_controller.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QMessageBox>
#include <QSignalBlocker>

#include <limits>

#pragma execution_character_set("utf-8")

InspectionStopController::InspectionStopController(Widget *host)
    : m_host(host)
{
}

void InspectionStopController::stopInspection()
{
    if (!m_host) {
        return;
    }

    qDebug() << "=== on_cancel_clicked() START ===";
    if (m_host->m_runtimeUiCoordinator) {
        m_host->m_runtimeUiCoordinator->clearDetectionRoiWarning(QString());
    }

    const bool recoveringInspectionFault =
            m_host->m_runtimeController.state()
            == InspectionRuntimeState::Fault;
    if (recoveringInspectionFault
            && !m_host->confirmInspectionFaultRecovery()) {
        m_host->presentInspectionFault();
        return;
    }

    const bool templateOperation =
            m_host->m_templateCaptureState
               != Widget::TemplateCaptureState::Idle
            || m_host->m_operationState
               == Widget::OperationState::TemplatePreviewing
            || m_host->m_operationState
               == Widget::OperationState::TemplateFrozen;
    if (templateOperation) {
        m_host->resetTemplateCaptureState();
        if (m_host->m_operationState == Widget::OperationState::Stopping) {
            m_host->ui->statusLabel->setText(
                        "正在退出模板制作，请稍候");
            return;
        }
        if (m_host->imageLabel) {
            m_host->imageLabel->setTemplateDrawingEnabled(false);
            m_host->imageLabel->clearSelection();
        }
        m_host->clearBarcodeTemplateValidation();
        m_host->hideTemplateGuide();
        m_host->ui->statusLabel->setText(
                    m_host->m_bOpenDevice
                    ? "已退出模板制作，相机已打开"
                    : "已退出模板制作，相机已关闭");
        m_host->updateOperationUiState();
        return;
    }

    if (m_host->m_operationState != Widget::OperationState::Detecting
            && !m_host->isCollecting
            && !m_host->hasRunningInspectionThread()
            && !m_host->m_runtimeController.isBusy()) {
        m_host->updateOperationUiState();
        return;
    }

    m_host->m_operationState = Widget::OperationState::Stopping;
    InspectionRuntimeStopTransaction stopTransaction(
                m_host->m_runtimeController);
    stopTransaction.begin();
    m_host->updateOperationUiState();
    m_host->m_barcodeWordRunActive = false;

    const InspectionAcquisitionStopResult acquisitionStopResult =
            m_host->m_acquisitionController
            ? m_host->m_acquisitionController->stopInspection()
            : InspectionAcquisitionStopResult();
    stopTransaction.waitForDetectionWorker();

    if (!acquisitionStopResult.allStopped()) {
        if (!acquisitionStopResult.softwareStopped) {
            qDebug() << "WARNING: software acquisition worker did not stop";
        }
        if (!acquisitionStopResult.hardwareStopped) {
            qDebug() << "WARNING: hardware acquisition worker did not stop";
        }
        m_host->ui->statusLabel->setText("停止中，请稍后再关闭相机");
        m_host->isCollecting = true;
        m_host->m_operationState = Widget::OperationState::Stopping;
        m_host->updateOperationUiState();
        return;
    }

    const InspectionCameraRecoveryResult cameraRecoveryResult =
            m_host->m_acquisitionController->recoverCamera(
                acquisitionStopResult.shouldRestoreCamera(),
                m_host->m_bOpenDevice,
                [this](QString *adjustmentMessage,
                       QString *errorMessage) {
        return m_host->applySavedCameraExposure(
                    adjustmentMessage,
                    errorMessage);
    });
    m_host->m_bOpenDevice = cameraRecoveryResult.cameraOpen;
    if (cameraRecoveryResult.issue
            == InspectionCameraRecoveryIssue::ExposureRejected) {
        {
            QSignalBlocker blocker(m_host->ui->spinBox);
            m_host->ui->spinBox->setRange(
                        0,
                        (std::numeric_limits<int>::max)());
            m_host->ui->spinBox->setValue(
                        m_host->m_appliedGlobalSettings.cameraExposure);
        }
        m_host->m_settingsPageController->refreshDirty("camera.exposure");
        QMessageBox::warning(m_host,
                    "警告",
                    QString("停止识别后恢复相机曝光失败：\n%1")
                    .arg(cameraRecoveryResult.errorMessage));
    } else if (cameraRecoveryResult.isRecovered()
               && cameraRecoveryResult.recoveryAttempted) {
        m_host->ui->statusLabel->setText("相机已打开");
        if (!cameraRecoveryResult.adjustmentMessage.isEmpty()) {
            QMessageBox::information(m_host,
                        "提示",
                        cameraRecoveryResult.adjustmentMessage);
        }
    }

    // Step 6: 处理事件队列
    QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);

    // Step 7: 清理临时绘制状态；生产统计保留，由“清零”按钮负责清空
    m_host->detectedRects.clear();
    m_host->selectionRect1 = QRect();

    if (m_host->imageLabel) {
        m_host->imageLabel->setTemplateDrawingEnabled(false);
        m_host->imageLabel->clearGreenRects();
        m_host->imageLabel->setColor(1);
        m_host->imageLabel->clearSelection();
    }
    m_host->hideTemplateGuide();

    if (m_host->m_resultCoordinator) {
        m_host->m_resultCoordinator->clear();
    }
    m_host->m_barcodeWordRunActive = false;

    m_host->first = false;

    m_host->ui->statusLabel->setText("已停止");
    m_host->isCollecting = false;
    stopTransaction.commit();
    if (!recoveringInspectionFault
            && m_host->m_runtimeController.state()
               == InspectionRuntimeState::Fault) {
        m_host->m_operationState = Widget::OperationState::Fault;
        m_host->presentInspectionFault();
        return;
    }
    if (recoveringInspectionFault) {
        QString reconciliationSummary;
        QString reconciliationError;
        if (!m_host->reconcileInspectionFaultProducts(
                    &reconciliationSummary,
                    &reconciliationError)) {
            m_host->m_operationState = Widget::OperationState::Fault;
            m_host->presentInspectionFault();
            QMessageBox::critical(m_host,
                        QStringLiteral("\u6545\u969c\u4ea7\u54c1\u6536\u53e3\u5931\u8d25"),
                        reconciliationError);
            return;
        }
        if (!m_host->m_runtimeController.acknowledgeFault()) {
            m_host->m_operationState = Widget::OperationState::Fault;
            m_host->presentInspectionFault();
            return;
        }
        m_host->restoreNormalFaultUi();
        if (!reconciliationSummary.isEmpty()) {
            QMessageBox::warning(m_host,
                        QStringLiteral("\u6545\u969c\u4ea7\u54c1\u6536\u53e3\u7ed3\u679c"),
                        reconciliationSummary);
        }
        qDebug() << "[INSPECTION_FAULT] operator acknowledged"
                 << "software runtime unlocked";
    }
    m_host->m_operationState = m_host->m_bOpenDevice
            ? Widget::OperationState::CameraReady
            : Widget::OperationState::CameraClosed;
    // 停止识别后保留最后一次判定结果、识别内容和耗时，
    // 便于现场人员复核。进入模板制作时仍会主动清除这些内容。
    m_host->updateOperationUiState();

    qDebug() << "=== on_cancel_clicked() COMPLETED ===";
}
