// 文件作用：本文件用于绑定检测页面控件，集中更新图像、判定、统计、耗时和运行按钮状态。
// 主要职责：绑定检测页面控件，集中更新图像、判定、统计、耗时和运行按钮状态。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "contracts/inspection_presentation.h"
#include "runtime/inspection_runtime.h"
#include "ui/controllers/operation_ui_policy.h"

#include <QString>

class QTimer;
class QWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class ImageLabel;

struct InspectionPageViewBindings
{
    ImageLabel *imageLabel_inspection = nullptr;
    QLabel *label_recognitionText = nullptr;
    QLabel *label_runtimeStatus = nullptr;
    QLabel *label_verdictResult = nullptr;
    QLineEdit *lineEdit_currentTemplateName = nullptr;
    QLineEdit *lineEdit_detectionDuration = nullptr;
    QLineEdit *lineEdit_ngCount = nullptr;
    QLineEdit *lineEdit_passRate = nullptr;
    QLineEdit *lineEdit_totalCount = nullptr;
    QPushButton *pushButton_saveTemplate = nullptr;
    QToolButton *toolButton_closeCamera = nullptr;
    QToolButton *toolButton_createTemplate = nullptr;
    QToolButton *toolButton_openCamera = nullptr;
    QToolButton *toolButton_startInspection = nullptr;
    QToolButton *toolButton_stopInspection = nullptr;
};

enum class InspectionClearScope
{
    ImageMetadata,
    AllDetectionData
};

// 组件说明：InspectionPage 组件负责对应界面区域的显示和用户交互。
class InspectionPage
{
public:
    InspectionPage(
        QWidget *rootWidget,
        const InspectionPageViewBindings &view,
        QTimer *templateAttentionTimer,
        bool *templateAttentionOn);
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
        bool *alarmPresented);
    bool confirmFaultRecovery(
        const InspectionFaultSnapshot &snapshot) const;
    void restoreNormalFaultStyle();
    void reportImageSaveFailure(
        quint64 totalFailed,
        const QString &latestError);

private:
    QWidget *m_rootWidget = nullptr;
    InspectionPageViewBindings m_view;
    QTimer *m_templateAttentionTimer = nullptr;
    bool *m_templateAttentionOn = nullptr;
    quint64 m_imageSaveFailedCount = 0;
    QString m_latestImageSaveError;
    bool m_imageSaveWarningScheduled = false;
};
