// 文件作用：本文件用于定义应用服务向检测界面发送的状态、结果和故障数据合同。
// 主要职责：定义应用服务向检测界面发送的状态、结果和故障数据合同。
// 模块位置：应用层；负责组织用户用例，并用结构化结果连接界面、运行时、配方和设置。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include <QDateTime>
#include <QImage>
#include <QString>
#include <QtGlobal>

#include <functional>

// 组件说明：InspectionVerdictStyleDto 枚举列出该组件允许使用的稳定状态和选项。
enum class InspectionVerdictStyleDto
{
    Correct,
    Incorrect
};

// 组件说明：InspectionViewBindingsDto 数据结构集中保存该流程需要的一组相关数据。
struct InspectionViewBindingsDto
{
    std::function<void(const QImage &)> showImage;
    std::function<void(InspectionVerdictStyleDto)> showVerdictStyle;
    std::function<void(const QString &)> showVerdictText;
    std::function<void(const QString &)> showRecognitionText;
    std::function<void(const QString &)> showTemplateName;
    std::function<void(int)> showTotalCount;
    std::function<void(int)> showNgCount;
    std::function<void(double)> showPassRate;
    std::function<void(const QString &)> showElapsedText;

    // 函数说明：isValid 函数检查相关状态并返回判断结果。
    bool isValid() const
    {
        return showImage && showVerdictStyle && showVerdictText
                && showRecognitionText && showTemplateName
                && showTotalCount && showNgCount
                && showPassRate && showElapsedText;
    }
};

// 组件说明：InspectionUiCallbacks 数据结构集中传递该流程需要的只读数据或回调。
struct InspectionUiCallbacks
{
    std::function<void()> warnMissingAnnotatedImage;
    std::function<void(quint64, const QString &)> reportImageSaveFailure;
    std::function<void(bool)> clearPreviousOverlay;
    std::function<void()> showDetectionRoiWarning;
    std::function<void()> clearDetectionRoiWarning;
};

// 组件说明：ApplicationFaultSnapshot 数据结构集中传递该流程需要的只读数据或回调。
struct ApplicationFaultSnapshot
{
    bool active = false;
    QString reasonText;
    QString diagnostic;
    QString runId;
    quint64 acceptedProductCount = 0;
    quint64 completedProductCount = 0;
    quint64 postFaultDroppedFrameCount = 0;
    QDateTime occurredAtUtc;

    // 函数说明：isActive 函数检查相关状态并返回判断结果。
    bool isActive() const { return active; }
};
