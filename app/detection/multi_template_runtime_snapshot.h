// 文件作用：把已加载模板转换为检测线程直接读取的只读快照。
#pragma once

#include "detection/detectionmode/barcode_word/barcode_word_detection_pipeline.h"
#include "detection/common/character_template_matcher.h"
#include "detection/common/detection_pose.h"
#include "engines/barcode/barcode_types.h"
#include "templates/template_store.h"

#include <QVector>

#include <vector>

struct MultiTemplateRuntimeConfig
{
    QString templateName;
    QStringList targetUnits;
    PreparedCharacterTemplates preparedTemplates;
    std::vector<int> templateTargetIndexes;
    int thresholdPercent = 0;
    BarcodeDecodeOptions barcodeOptions;
    BarcodeWordDecodeStrategyState decodeStrategy;
};

struct MultiTemplateRuntimeSnapshot
{
    std::vector<WordTrackingTemplate> trackingTemplates;
    std::vector<MultiTemplateRuntimeConfig> runtimeConfigs;

    bool isValid() const
    {
        return !trackingTemplates.empty()
                && trackingTemplates.size() == runtimeConfigs.size();
    }
};

class MultiTemplateRuntimeSnapshotBuilder
{
public:
    static MultiTemplateRuntimeSnapshot create(
        const QVector<PreparedTemplateSnapshot> &templates);
};
