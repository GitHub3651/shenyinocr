// 文件作用：本文件用于把配方Profile准备为检测线程可直接读取的只读运行快照。
// 主要职责：把配方Profile准备为检测线程可直接读取的只读运行快照。
// 模块位置：检测层；把PreparedRecipe转换为检测线程只读快照。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "engines/barcode/barcode_types.h"
#include "detection/positioning/detection_pose.h"
#include "detection/barcode_word/barcode_word_detection_pipeline.h"
#include "detection/common/character_template_matcher.h"
#include "recipes/prepared_recipe.h"

#include <QString>

#include <vector>

// 组件说明：DetectionModeWorkerProfile 数据结构集中保存该流程需要的一组相关数据。
struct DetectionModeWorkerProfile
{
    QString templateName;
    QString targetText;
    PreparedCharacterTemplates preparedTemplates;
    std::vector<int> templateTargetIndexes;
    int thresholdPercent = 0;
    BarcodeDecodeOptions barcodeOptions;
    BarcodeWordDecodeStrategyState decodeStrategy;
};

// 组件说明：DetectionProfileSnapshot是检测线程使用的只读Profile快照。
struct DetectionProfileSnapshot
{
    std::vector<WordTrackingProfile> trackingProfiles;
    std::vector<DetectionModeWorkerProfile> detectionProfiles;

    // 函数说明：isValid 函数检查相关状态并返回判断结果。
    bool isValid() const
    {
        return !trackingProfiles.empty()
                && trackingProfiles.size() == detectionProfiles.size();
    }

};

// 组件说明：DetectionProfileSnapshotBuilder集中完成运行Profile准备。
class DetectionProfileSnapshotBuilder
{
public:
    static DetectionProfileSnapshot create(
        const PreparedRecipe &preparedRecipe);
};
