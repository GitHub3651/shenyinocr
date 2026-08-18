// 文件作用：本文件用于对比主检测结果和影子检测结果，记录差异而不改变正式判定。
// 主要职责：对比主检测结果和影子检测结果，记录差异而不改变正式判定。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/positioning/detection_pose.h"

#include <QStringList>

// 组件说明：DetectionShadowComparisonOptions 组件集中描述相关配置、规则和运行参数。
struct DetectionShadowComparisonOptions
{
    int coordinateTolerance = 0;
    double scoreTolerance = 0.0;
    bool compareDiagnostic = false;
};

// 组件说明：DetectionShadowComparison 数据结构集中保存该流程需要的一组相关数据。
struct DetectionShadowComparison
{
    QStringList differences;

    // 函数说明：isEquivalent 函数检查相关状态并返回判断结果。
    bool isEquivalent() const
    {
        return differences.isEmpty();
    }
};

// 组件说明：DetectionShadowComparator 组件封装本文件中与其名称对应的单一职责。
class DetectionShadowComparator
{
public:
    static DetectionShadowComparison compare(
        const DetectionResult &primary,
        const DetectionResult &shadow,
        const DetectionShadowComparisonOptions &options =
            DetectionShadowComparisonOptions());
};
