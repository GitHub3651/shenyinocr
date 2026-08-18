// 文件作用：本文件用于绑定检测页面控件，集中更新图像、判定、统计、耗时和运行按钮状态。
// 主要职责：绑定检测页面控件，集中更新图像、判定、统计、耗时和运行按钮状态。
// 模块位置：界面层；负责收集用户操作和显示应用层返回的数据，不拥有设备或生产线程。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "application/inspection_ui_contract.h"
#include "ui/controllers/operation_ui_policy.h"

#include <QString>

#include <functional>

class QTimer;
// 组件说明：QWidget 组件封装本文件中与其名称对应的单一职责。
class QWidget;

namespace Ui {
// 组件说明：MainWindow 组件负责对应界面区域的显示和用户交互。
class MainWindow;
}

// 组件说明：InspectionPage 组件负责对应界面区域的显示和用户交互。
class InspectionPage
{
public:
    // 组件说明：Callbacks 数据结构集中传递该流程需要的只读数据或回调。
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
