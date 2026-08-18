#include "ui/pages/inspection_page.h"

#include "ui_main_window.h"
#include "ui/presenters/inspection_fault_presenter.h"

#include <QAbstractButton>
#include <QDebug>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QStyle>
#include <QTimer>
#include <QWidget>

namespace {

void setLabelTextIfChanged(QLabel *label, const QString &text)
{
    if (label && label->text() != text) {
        label->setText(text);
    }
}

}

InspectionPage::InspectionPage(
    QWidget *rootWidget,
    Ui::MainWindow *ui,
    QTimer *templateAttentionTimer,
    bool *templateAttentionOn,
    const Callbacks &callbacks)
    : m_rootWidget(rootWidget),
      m_ui(ui),
      m_templateAttentionTimer(templateAttentionTimer),
      m_templateAttentionOn(templateAttentionOn),
      m_callbacks(callbacks)
{
}

InspectionViewBindingsDto
InspectionPage::resultViewBindings() const
{
    InspectionViewBindingsDto bindings;
    bindings.showImage = [this](const QImage &image) {
        if (!m_ui || !m_ui->imageLabel_inspection) {
            return;
        }
        const QPixmap pixmap = QPixmap::fromImage(image);
        m_ui->imageLabel_inspection->setScaledContents(false);
        m_ui->imageLabel_inspection->setAlignment(Qt::AlignCenter);
        m_ui->imageLabel_inspection->setAutoFitPixmap(pixmap);
        if (!m_ui->imageLabel_inspection->isTemplateDrawingEnabled()
                && m_callbacks.updateImageDisplayStatus) {
            m_callbacks.updateImageDisplayStatus(
                        QStringLiteral(
                            "\u6b63\u5728\u663e\u793a"
                            "\u76f8\u673a\u91c7\u96c6\u56fe\u50cf..."));
        }
    };
    bindings.showVerdictStyle = [this](
            InspectionVerdictStyleDto style) {
        if (!m_ui || !m_ui->label_verdictResult) {
            return;
        }
        const QString color =
                style == InspectionVerdictStyleDto::Correct
                ? QStringLiteral("#00ff7f")
                : QStringLiteral("#ff0000");
        m_ui->label_verdictResult->setTextFormat(Qt::PlainText);
        m_ui->label_verdictResult->setStyleSheet(
                    QStringLiteral(
                        "background-color: #eef1f6; "
                        "border-radius: 6px; "
                        "font-size: 36px; "
                        "font-weight: 900; "
                        "color: %1;")
                    .arg(color));
        m_ui->label_verdictResult->setWordWrap(true);
    };
    bindings.showVerdictText = [this](const QString &text) {
        if (m_ui) {
            setLabelTextIfChanged(m_ui->label_verdictResult, text);
        }
    };
    bindings.showRecognitionText = [this](const QString &text) {
        if (m_ui) {
            setLabelTextIfChanged(m_ui->label_recognitionText, text);
        }
    };
    bindings.showTemplateName = [this](const QString &text) {
        if (m_ui && m_ui->lineEdit_currentRecipeName) {
            m_ui->lineEdit_currentRecipeName->setText(text);
        }
    };
    bindings.showTotalCount = [this](int count) {
        if (m_ui && m_ui->lineEdit_totalCount) {
            m_ui->lineEdit_totalCount->setText(QString::number(count));
        }
    };
    bindings.showNgCount = [this](int count) {
        if (m_ui && m_ui->lineEdit_ngCount) {
            m_ui->lineEdit_ngCount->setText(QString::number(count));
        }
    };
    bindings.showPassRate = [this](double passRate) {
        if (m_ui && m_ui->lineEdit_passRate) {
            m_ui->lineEdit_passRate->setText(
                        QString::number(passRate, 'f', 1));
        }
    };
    bindings.showElapsedText = [this](const QString &text) {
        if (m_ui && m_ui->lineEdit_detectionDuration) {
            m_ui->lineEdit_detectionDuration->setText(text);
        }
    };
    return bindings;
}

void InspectionPage::updateOperationState(
    OperationUiState requestedState,
    bool runtimeFaulted)
{
    if (!m_ui || !m_rootWidget) {
        return;
    }

    const QList<QAbstractButton *> operationButtons =
            m_rootWidget->findChildren<QAbstractButton *>();
    for (QAbstractButton *button : operationButtons) {
        if (!button) {
            continue;
        }
        const QString marker = QStringLiteral(
                    "/* operation-disabled-style */");
        if (!button->styleSheet().contains(marker)) {
            button->setStyleSheet(
                        button->styleSheet()
                        + QStringLiteral(
                            "\n/* operation-disabled-style */"
                            "QPushButton:disabled,"
                            "QToolButton:disabled,"
                            "QCheckBox:disabled {"
                            "background-color: #f2f3f5;"
                            "color: #a8abb2;"
                            "border-color: #dcdfe6;"
                            "}"));
        }
        button->setEnabled(false);
    }

    const OperationUiState effectiveState = runtimeFaulted
            ? OperationUiState::Fault
            : requestedState;
    const OperationUiSnapshot operationUi =
            OperationUiPolicy::create(effectiveState);
    if (operationUi.enableAllOperations) {
        for (QAbstractButton *button : operationButtons) {
            if (button) {
                button->setEnabled(true);
            }
        }
    }

    m_ui->toolButton_startInspection->setText(operationUi.startDetectionText);
    m_ui->toolButton_stopInspection->setText(operationUi.stopText);
    m_ui->toolButton_createTemplate->setText(operationUi.templateCaptureText);
    m_ui->toolButton_openCamera->setEnabled(operationUi.openCameraEnabled);
    m_ui->toolButton_startInspection->setEnabled(operationUi.startDetectionEnabled);
    m_ui->toolButton_stopInspection->setEnabled(operationUi.stopEnabled);
    m_ui->toolButton_closeCamera->setEnabled(operationUi.closeCameraEnabled);
    m_ui->toolButton_createTemplate->setEnabled(operationUi.templateCaptureEnabled);
    m_ui->pushButton_saveTemplate->setEnabled(operationUi.saveTemplateEnabled);
    if (!operationUi.statusText.isEmpty()) {
        m_ui->label_runtimeStatus->setText(operationUi.statusText);
    }

    if (m_callbacks.setSettingsEnabled) {
        m_callbacks.setSettingsEnabled(operationUi.settingsEnabled);
    }

    if (m_templateAttentionTimer && m_ui->toolButton_createTemplate) {
        if (requestedState == OperationUiState::TemplatePreviewing) {
            if (!m_templateAttentionTimer->isActive()) {
                if (m_templateAttentionOn) {
                    *m_templateAttentionOn = true;
                }
                m_ui->toolButton_createTemplate->setProperty(
                            "templateCaptureActive", true);
                m_ui->toolButton_createTemplate->setProperty(
                            "templateCaptureAttention", true);
                m_ui->toolButton_createTemplate->style()->unpolish(m_ui->toolButton_createTemplate);
                m_ui->toolButton_createTemplate->style()->polish(m_ui->toolButton_createTemplate);
                m_ui->toolButton_createTemplate->update();
                m_templateAttentionTimer->start();
            }
        } else {
            m_templateAttentionTimer->stop();
            const bool attentionOn = m_templateAttentionOn
                    && *m_templateAttentionOn;
            if (attentionOn
                    || m_ui->toolButton_createTemplate->property(
                        "templateCaptureActive").toBool()
                    || m_ui->toolButton_createTemplate->property(
                        "templateCaptureAttention").toBool()) {
                if (m_templateAttentionOn) {
                    *m_templateAttentionOn = false;
                }
                m_ui->toolButton_createTemplate->setProperty(
                            "templateCaptureActive", false);
                m_ui->toolButton_createTemplate->setProperty(
                            "templateCaptureAttention", false);
                m_ui->toolButton_createTemplate->style()->unpolish(m_ui->toolButton_createTemplate);
                m_ui->toolButton_createTemplate->style()->polish(m_ui->toolButton_createTemplate);
                m_ui->toolButton_createTemplate->update();
            }
        }
    }

    if (operationUi.enableAllOperations
            && m_callbacks.refreshHardwareSettingsEnabled) {
        m_callbacks.refreshHardwareSettingsEnabled();
    }
}

void InspectionPage::presentFault(
    const ApplicationFaultSnapshot &snapshot,
    bool *alarmPresented)
{
    const InspectionFaultPresentation presentation =
            InspectionFaultPresenter::create(snapshot);
    if (!presentation.isValid() || !m_ui) {
        return;
    }
    m_ui->label_runtimeStatus->setText(presentation.statusText);
    m_ui->label_runtimeStatus->setStyleSheet(
                presentation.statusStyleSheet);
    m_ui->label_verdictResult->setTextFormat(Qt::PlainText);
    m_ui->label_verdictResult->setText(presentation.resultText);
    m_ui->label_verdictResult->setStyleSheet(
                presentation.resultStyleSheet);

    if (alarmPresented && !*alarmPresented) {
        *alarmPresented = true;
        QMessageBox::critical(
                    m_rootWidget,
                    QStringLiteral("\u7cfb\u7edf\u6545\u969c\uff0d\u68c0\u6d4b\u5df2\u6682\u505c"),
                    presentation.operatorMessage);
    }
}

bool InspectionPage::confirmFaultRecovery(
    const ApplicationFaultSnapshot &snapshot) const
{
    const InspectionFaultPresentation presentation =
            InspectionFaultPresenter::create(snapshot);
    if (!presentation.isValid()) {
        return false;
    }

    QMessageBox messageBox(
                QMessageBox::Critical,
                QStringLiteral("\u6545\u969c\u6062\u590d\u786e\u8ba4"),
                presentation.operatorMessage
                + QStringLiteral(
                    "\n\n\u6ce8\u610f\uff1a\u89e3\u9664\u8f6f\u4ef6\u9501\u5b9a\u4e0d\u4ee3\u8868\u8f93\u9001\u7ebf\u5df2\u505c\u6b62\u3002"),
                QMessageBox::Yes | QMessageBox::Cancel,
                m_rootWidget);
    messageBox.setDefaultButton(QMessageBox::Cancel);
    if (QAbstractButton *confirmButton =
            messageBox.button(QMessageBox::Yes)) {
        confirmButton->setText(
                    QStringLiteral("\u786e\u8ba4\u73b0\u573a\u5df2\u5904\u7406\u5e76\u6062\u590d"));
    }
    if (QAbstractButton *cancelButton =
            messageBox.button(QMessageBox::Cancel)) {
        cancelButton->setText(
                    QStringLiteral("\u7ee7\u7eed\u4fdd\u6301\u6545\u969c\u9501\u5b9a"));
    }
    return messageBox.exec() == QMessageBox::Yes;
}

void InspectionPage::restoreNormalFaultStyle()
{
    if (!m_ui) {
        return;
    }
    m_ui->label_runtimeStatus->setStyleSheet(
                QStringLiteral(
                    "QLabel{color:#2ecc71; font-weight:bold;}"));
    m_ui->label_verdictResult->setStyleSheet(
                QStringLiteral(
                    "background-color: #eef1f6; "
                    "border-radius: 6px; "
                    "font-size: 36px; "
                    "font-weight: 900; "
                    "color: #00ff7f;"));
}

void InspectionPage::showDetectionRoiWarning()
{
    if (m_detectionRoiWarningActive) {
        return;
    }
    m_detectionRoiWarningActive = true;
    const QString warningText = QString::fromWCharArray(
                L"\u8bc6\u522b\u533a\u57df\u8d85\u51fa\u539f\u56fe\u8303\u56f4\uff0c"
                L"\u8bf7\u70b9\u51fb\u3010\u505c\u6b62\u8bc6\u522b\u3011\uff0c"
                L"\u7136\u540e\u91cd\u65b0\u9009\u62e9\u6216\u5236\u4f5c\u6a21\u677f\u3002");
    qWarning().noquote() << "[DETECTION_ROI]" << warningText;
    if (m_ui && m_ui->label_runtimeStatus) {
        m_ui->label_runtimeStatus->setWordWrap(true);
        m_ui->label_runtimeStatus->setText(warningText);
        m_ui->label_runtimeStatus->setStyleSheet(
                    QStringLiteral(
                        "QLabel{color:#d90000;font-weight:900;}"));
    }
}

void InspectionPage::clearDetectionRoiWarning(
    const QString &runningStatusText)
{
    if (!m_detectionRoiWarningActive) {
        return;
    }
    m_detectionRoiWarningActive = false;
    if (m_ui && m_ui->label_runtimeStatus
            && !runningStatusText.isEmpty()) {
        m_ui->label_runtimeStatus->setText(runningStatusText);
        m_ui->label_runtimeStatus->setStyleSheet(
                    QStringLiteral(
                        "QLabel{color:#20b455;font-weight:bold;}"));
    }
}

void InspectionPage::warnMissingAnnotatedImage() const
{
    QMessageBox::warning(
                m_rootWidget,
                QString::fromWCharArray(L"\u8b66\u544a"),
                QString::fromWCharArray(
                    L"\u4fdd\u5b58\u5931\u8d25,"
                    L"\u672a\u91c7\u96c6\u5230\u56fe\u50cf\uff01"));
}

void InspectionPage::reportImageSaveFailure(
    quint64 totalFailed,
    const QString &latestError)
{
    m_imageSaveFailedCount = totalFailed;
    m_latestImageSaveError = latestError;
    if (m_imageSaveWarningScheduled || !m_rootWidget) {
        return;
    }
    m_imageSaveWarningScheduled = true;
    QTimer::singleShot(250, m_rootWidget, [this]() {
        m_imageSaveWarningScheduled = false;
        QString warningText = QString::fromWCharArray(
                    L"\u5b58\u56fe\u5931\u8d25\uff1a\u7d2f\u8ba1 %1 \u4e2a\u4efb\u52a1\u3002"
                    L"\u8bf7\u68c0\u67e5\u5b58\u56fe\u76ee\u5f55\u3001\u6743\u9650\u548c\u78c1\u76d8\u7a7a\u95f4\u3002")
                .arg(m_imageSaveFailedCount);
        if (!m_latestImageSaveError.trimmed().isEmpty()) {
            warningText += QString::fromWCharArray(
                        L"\n\u6700\u8fd1\u9519\u8bef\uff1a%1")
                    .arg(m_latestImageSaveError);
        }
        qWarning().noquote() << "[IMAGE_SAVE]" << warningText;
        if (m_ui && m_ui->label_runtimeStatus) {
            m_ui->label_runtimeStatus->setWordWrap(true);
            m_ui->label_runtimeStatus->setText(warningText);
            m_ui->label_runtimeStatus->setStyleSheet(
                        QStringLiteral(
                            "QLabel{color:#d90000;font-weight:900;}"));
        }
    });
}
