// 文件作用：本文件用于绑定检测页面控件，集中更新图像、判定、统计、耗时和运行按钮状态。
// 主要职责：绑定检测页面控件，集中更新图像、判定、统计、耗时和运行按钮状态。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "ui/pages/inspection_page.h"

#include "ui/presenters/inspection_fault_presenter.h"
#include "ui/widgets/image_label.h"

#include <QAbstractButton>
#include <QDebug>
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

// 函数说明：setLabelTextIfChanged 函数更新或应用对应的配置和状态。
void setLabelTextIfChanged(QLabel *label, const QString &text)
{
    if (label && label->text() != text) {
        label->setText(text);
    }
}

void setStyleProperty(
    QWidget *widget,
    const char *name,
    const QVariant &value)
{
    if (!widget || widget->property(name) == value) {
        return;
    }
    widget->setProperty(name, value);
    if (widget->style()) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    widget->update();
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

// 函数说明：InspectionPage 构造函数创建组件并初始化其依赖和初始状态。
InspectionPage::InspectionPage(
    QWidget *rootWidget,
    const InspectionPageViewBindings &view,
    QTimer *templateAttentionTimer,
    bool *templateAttentionOn,
    const Callbacks &callbacks)
    : m_rootWidget(rootWidget),
      m_view(view),
      m_templateAttentionTimer(templateAttentionTimer),
      m_templateAttentionOn(templateAttentionOn),
      m_callbacks(callbacks)
{
    if (m_view.toolButton_createTemplate) {
        m_view.toolButton_createTemplate->setToolTip(
                    QStringLiteral("进入当前检测模式的模板制作流程。"));
    }
}

InspectionViewBindingsDto
// 函数说明：resultViewBindings 函数实现名称所表示的处理步骤。
InspectionPage::resultViewBindings() const
{
    InspectionViewBindingsDto bindings;
    bindings.showImage = [this](const QImage &image) {
        if (!m_view.label_runtimeStatus || !m_view.imageLabel_inspection) {
            return;
        }
        const QPixmap pixmap = QPixmap::fromImage(image);
        m_view.imageLabel_inspection->setScaledContents(false);
        m_view.imageLabel_inspection->setAlignment(Qt::AlignCenter);
        m_view.imageLabel_inspection->setAutoFitPixmap(pixmap);
        if (!m_view.imageLabel_inspection->isTemplateDrawingEnabled()
                && m_callbacks.updateImageDisplayStatus) {
            m_callbacks.updateImageDisplayStatus(
                        QStringLiteral(
                            "正在显示"
                            "相机采集图像..."));
        }
    };
    bindings.showVerdictStyle = [this](
            InspectionVerdictStyleDto style) {
        if (!m_view.label_runtimeStatus || !m_view.label_verdictResult) {
            return;
        }
        m_view.label_verdictResult->setTextFormat(Qt::PlainText);
        setStyleProperty(
                    m_view.label_verdictResult,
                    "verdict",
                    style == InspectionVerdictStyleDto::Correct
                    ? QStringLiteral("ok")
                    : QStringLiteral("ng"));
        m_view.label_verdictResult->setWordWrap(true);
    };
    bindings.showVerdictText = [this](const QString &text) {
        if (m_view.label_runtimeStatus) {
            setLabelTextIfChanged(m_view.label_verdictResult, text);
        }
    };
    bindings.showRecognitionText = [this](const QString &text) {
        if (m_view.label_runtimeStatus) {
            setLabelTextIfChanged(m_view.label_recognitionText, text);
        }
    };
    bindings.showTemplateName = [this](const QString &text) {
        if (m_view.label_runtimeStatus && m_view.lineEdit_currentTemplateName) {
            m_view.lineEdit_currentTemplateName->setText(text);
        }
    };
    bindings.showTotalCount = [this](int count) {
        if (m_view.label_runtimeStatus && m_view.lineEdit_totalCount) {
            m_view.lineEdit_totalCount->setText(QString::number(count));
        }
    };
    bindings.showNgCount = [this](int count) {
        if (m_view.label_runtimeStatus && m_view.lineEdit_ngCount) {
            m_view.lineEdit_ngCount->setText(QString::number(count));
        }
    };
    bindings.showPassRate = [this](double passRate) {
        if (m_view.label_runtimeStatus && m_view.lineEdit_passRate) {
            m_view.lineEdit_passRate->setText(
                        QString::number(passRate, 'f', 1));
        }
    };
    bindings.showElapsedText = [this](const QString &text) {
        if (m_view.label_runtimeStatus && m_view.lineEdit_detectionDuration) {
            m_view.lineEdit_detectionDuration->setText(text);
        }
    };
    return bindings;
}

// 函数说明：applyOperationState 根据统一权限快照更新检测页控件。
void InspectionPage::applyOperationState(
    OperationUiState requestedState,
    const OperationUiSnapshot &operationUi)
{
    if (!m_view.label_runtimeStatus || !m_rootWidget) {
        return;
    }

    m_view.toolButton_startInspection->setText(operationUi.startDetectionText);
    m_view.toolButton_stopInspection->setText(operationUi.stopText);
    m_view.toolButton_createTemplate->setText(operationUi.templateCaptureText);
    applyOperationUiAccess(m_view.toolButton_openCamera, operationUi.openCamera);
    applyOperationUiAccess(
                m_view.toolButton_startInspection,
                operationUi.startDetection);
    applyOperationUiAccess(m_view.toolButton_stopInspection, operationUi.stop);
    applyOperationUiAccess(m_view.toolButton_closeCamera, operationUi.closeCamera);
    applyOperationUiAccess(
                m_view.toolButton_createTemplate,
                operationUi.templateCapture);
    applyOperationUiAccess(
                m_view.pushButton_saveTemplate,
                operationUi.saveTemplate);
    if (!operationUi.statusText.isEmpty()) {
        m_view.label_runtimeStatus->setText(operationUi.statusText);
    }
    const bool keepDetectionWarning =
            requestedState == OperationUiState::Detecting
            && m_view.label_runtimeStatus->property("uiState").toString()
               == QStringLiteral("warning")
            && operationUi.statusText.isEmpty();
    if (!keepDetectionWarning) {
        setStyleProperty(
                    m_view.label_runtimeStatus,
                    "uiState",
                    runtimeUiState(requestedState));
    }

    if (m_templateAttentionTimer && m_view.toolButton_createTemplate) {
        if (requestedState == OperationUiState::TemplatePreviewing) {
            if (!m_templateAttentionTimer->isActive()) {
                if (m_templateAttentionOn) {
                    *m_templateAttentionOn = true;
                }
                setStyleProperty(
                            m_view.toolButton_createTemplate,
                            "uiState",
                            QStringLiteral("attention"));
                m_templateAttentionTimer->start();
            }
        } else {
            m_templateAttentionTimer->stop();
            const bool attentionOn = m_templateAttentionOn
                    && *m_templateAttentionOn;
            if (attentionOn
                    || m_view.toolButton_createTemplate->property(
                        "uiState").toString()
                       == QStringLiteral("preview")
                    || m_view.toolButton_createTemplate->property(
                        "uiState").toString()
                       == QStringLiteral("attention")) {
                if (m_templateAttentionOn) {
                    *m_templateAttentionOn = false;
                }
                setStyleProperty(
                            m_view.toolButton_createTemplate,
                            "uiState",
                            QString());
            }
        }
    }

}

// 函数说明：presentFault 函数执行对应事件或业务处理。
void InspectionPage::presentFault(
    const ApplicationFaultSnapshot &snapshot,
    bool *alarmPresented)
{
    const InspectionFaultPresentation presentation =
            InspectionFaultPresenter::create(snapshot);
    if (!presentation.isValid() || !m_view.label_runtimeStatus) {
        return;
    }
    m_view.label_runtimeStatus->setText(presentation.statusText);
    setStyleProperty(
                m_view.label_runtimeStatus,
                "uiState",
                QStringLiteral("fault"));
    m_view.label_verdictResult->setTextFormat(Qt::PlainText);
    m_view.label_verdictResult->setText(presentation.resultText);
    setStyleProperty(
                m_view.label_verdictResult,
                "verdict",
                QStringLiteral("fault"));

    if (alarmPresented && !*alarmPresented) {
        *alarmPresented = true;
        QMessageBox::critical(
                    m_rootWidget,
                    QStringLiteral("系统故障－检测已暂停"),
                    presentation.operatorMessage);
    }
}

// 函数说明：confirmFaultRecovery 函数实现名称所表示的处理步骤。
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
                QStringLiteral("故障恢复确认"),
                presentation.operatorMessage
                + QStringLiteral(
                    "\n\n注意：解除软件锁定不代表输送线已停止。"),
                QMessageBox::Yes | QMessageBox::Cancel,
                m_rootWidget);
    messageBox.setDefaultButton(QMessageBox::Cancel);
    if (QAbstractButton *confirmButton =
            messageBox.button(QMessageBox::Yes)) {
        confirmButton->setText(
                    QStringLiteral("确认现场已处理并恢复"));
    }
    if (QAbstractButton *cancelButton =
            messageBox.button(QMessageBox::Cancel)) {
        cancelButton->setText(
                    QStringLiteral("继续保持故障锁定"));
    }
    return messageBox.exec() == QMessageBox::Yes;
}

// 函数说明：restoreNormalFaultStyle 函数校验、转换或恢复对应数据。
void InspectionPage::restoreNormalFaultStyle()
{
    if (!m_view.label_runtimeStatus) {
        return;
    }
    setStyleProperty(
                m_view.label_runtimeStatus,
                "uiState",
                QStringLiteral("ready"));
    setStyleProperty(
                m_view.label_verdictResult,
                "verdict",
                QStringLiteral("idle"));
}

// 函数说明：showDetectionRoiWarning 函数实现名称所表示的处理步骤。
void InspectionPage::showDetectionRoiWarning()
{
    if (m_detectionRoiWarningActive) {
        return;
    }
    m_detectionRoiWarningActive = true;
    const QString warningText = QString::fromWCharArray(
                L"识别区域超出原图范围，"
                L"请点击【停止识别】，"
                L"然后重新选择或制作模板。");
    qWarning().noquote() << "[DETECTION_ROI]" << warningText;
    if (m_view.label_runtimeStatus) {
        m_view.label_runtimeStatus->setWordWrap(true);
        m_view.label_runtimeStatus->setText(warningText);
        setStyleProperty(
                    m_view.label_runtimeStatus,
                    "uiState",
                    QStringLiteral("warning"));
    }
}

// 函数说明：clearDetectionRoiWarning 函数停止流程、清理状态或释放对应资源。
void InspectionPage::clearDetectionRoiWarning(
    const QString &runningStatusText)
{
    if (!m_detectionRoiWarningActive) {
        return;
    }
    m_detectionRoiWarningActive = false;
    if (m_view.label_runtimeStatus && !runningStatusText.isEmpty()) {
        m_view.label_runtimeStatus->setText(runningStatusText);
        setStyleProperty(
                    m_view.label_runtimeStatus,
                    "uiState",
                    QStringLiteral("running"));
    }
}

// 函数说明：warnMissingAnnotatedImage 函数实现名称所表示的处理步骤。
void InspectionPage::warnMissingAnnotatedImage() const
{
    QMessageBox::warning(
                m_rootWidget,
                QString::fromWCharArray(L"警告"),
                QString::fromWCharArray(
                    L"保存失败,"
                    L"未采集到图像！"));
}

// 函数说明：reportImageSaveFailure 函数实现名称所表示的处理步骤。
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
                    L"存图失败：累计 %1 个任务。"
                    L"请检查存图目录、权限和磁盘空间。")
                .arg(m_imageSaveFailedCount);
        if (!m_latestImageSaveError.trimmed().isEmpty()) {
            warningText += QString::fromWCharArray(
                        L"\n最近错误：%1")
                    .arg(m_latestImageSaveError);
        }
        qWarning().noquote() << "[IMAGE_SAVE]" << warningText;
        if (m_view.label_runtimeStatus) {
            m_view.label_runtimeStatus->setWordWrap(true);
            m_view.label_runtimeStatus->setText(warningText);
            setStyleProperty(
                        m_view.label_runtimeStatus,
                        "uiState",
                        QStringLiteral("warning"));
        }
    });
}
