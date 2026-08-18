// 文件作用：本文件用于定义一次产品检测在界面上完整呈现所需的只读快照。
// 主要职责：定义一次产品检测在界面上完整呈现所需的只读快照。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/positioning/detection_pose.h"

#include <QImage>
#include <QString>

// 组件说明：DetectionVerdictViewStyle 枚举列出该组件允许使用的稳定状态和选项。
enum class DetectionVerdictViewStyle
{
    Correct,
    Error
};

// 组件说明：DetectionResultStatistics 数据结构集中保存该流程需要的一组相关数据。
struct DetectionResultStatistics
{
    int totalCount = 0;
    int ngCount = 0;

    // 函数说明：passRatePercent 函数实现名称所表示的处理步骤。
    double passRatePercent() const
    {
        return totalCount > 0
                ? (1.0 - static_cast<double>(ngCount) / totalCount) * 100.0
                : 0.0;
    }
};

// 组件说明：DetectionAbnormalStatistics 数据结构集中保存该流程需要的一组相关数据。
struct DetectionAbnormalStatistics
{
    quint64 systemFaultCount = 0;
    quint64 unconfirmedProductCount = 0;
    quint64 postFaultDroppedFrameCount = 0;
};

// Complete read-only product presentation. The UI applies this object as one
// unit so image, verdict, text, template, statistics and timing cannot drift
// between different ProductKeys.
struct InspectionPresentation
{
    ProductKey productKey;
    QImage image;
    DetectionVerdictViewStyle verdictStyle =
            DetectionVerdictViewStyle::Error;
    QString recognitionText;
    bool updatesTemplateName = false;
    QString templateName;
    DetectionResultStatistics statistics;
    QString elapsedText;

    // 函数说明：isValid 函数检查相关状态并返回判断结果。
    bool isValid() const
    {
        return productKey.isValid() && !image.isNull();
    }
};
