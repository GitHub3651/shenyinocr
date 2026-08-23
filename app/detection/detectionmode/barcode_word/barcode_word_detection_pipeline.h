// 文件作用：本文件用于执行二维码与三期字符组合模式的定位、解码、字符检查和结果生成。
// 主要职责：执行二维码与三期字符组合模式的定位、解码、字符检查和结果生成。
// 模块位置：检测层；只处理图像、定位和判定，不访问界面、磁盘、PLC或相机SDK。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#ifndef DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H
#define DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H

#include "engines/barcode/barcode_types.h"
#include "detection/detectionmode/word/word_detection_pipeline.h"

#include <QString>

#include <functional>
#include <vector>

// 组件说明：IBarcodeDecoder 组件提供对应设备或检测能力的统一实现。
class IBarcodeDecoder;

// 组件说明：BarcodeWordDateDetectionResult 数据结构保存一次操作的结果、状态和错误信息。
struct BarcodeWordDateDetectionResult
{
    bool resultProduced = false;
    bool isOk = false;
};

// 组件说明：BarcodeWordDetectionResult 数据结构保存一次操作的结果、状态和错误信息。
struct BarcodeWordDetectionResult
{
    bool barcodeIsReadable = false;
    bool dateDetectionExecuted = false;
    bool dateResultProduced = false;
    bool dateIsOk = false;
    bool isOk = false;
};

// 组件说明：BarcodeWordDecodeStrategyState 数据结构集中保存该流程需要的一组相关数据。
struct BarcodeWordDecodeStrategyState
{
    int preferredStrategyId = -1;
    unsigned int preferredOptionFlags =
            BarcodeDecodeOptionFlags::None;
    int consecutiveFailures = 0;
};

// 组件说明：BarcodeWordDetectionWorkOutput 数据结构集中保存该流程需要的一组相关数据。
struct BarcodeWordDetectionWorkOutput
{
    DetectionResult detectionResult;
    BarcodeWordDetectionResult barcodeWordResult;
    WordDetectionWorkOutput wordOutput;
    DetectionPose pose;
    QString templateName;
    BarcodeReadResult barcode;
    BarcodeWordDecodeStrategyState nextDecodeStrategy;
    QString barcodeState;
    QString dateState;
    QString reason;
    bool barcodeRoiValid = false;
    bool dateRoiValid = false;
};

// 组件说明：BarcodeWordDetectionPipeline 组件提供对应设备或检测能力的统一实现。
class BarcodeWordDetectionPipeline
{
public:
    typedef std::function<BarcodeWordDateDetectionResult()>
            DateDetectionFunction;

    BarcodeWordDetectionResult detect(
            bool barcodeIsReadable,
            const DateDetectionFunction &detectDate) const;

    BarcodeWordDetectionWorkOutput detect(
            const DetectionWorkItem &item,
            const QString &targetText,
            const QString &templateName,
            const PreparedCharacterTemplates &preparedTemplates,
            const std::vector<int> &templateTargetIndexes,
            int thresholdPercent,
            const BarcodeDecodeOptions &decodeOptions,
            const BarcodeWordDecodeStrategyState &decodeStrategy,
            IBarcodeDecoder *decoder) const;
};

#endif // DETECTION_BARCODE_WORD_BARCODE_WORD_DETECTION_PIPELINE_H
