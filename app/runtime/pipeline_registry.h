// 文件作用：本文件用于按稳定检测模式注册并选择唯一检测流水线。
// 主要职责：按稳定检测模式注册并选择唯一检测流水线。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "devices/barcode/barcode_types.h"
#include "detection/barcode_word/barcode_word_detection_pipeline.h"
#include "detection/common/character_template_matcher.h"
#include "detection/stamp/stamp_detection_pipeline.h"
#include "detection/tissue/tissue_detection_pipeline.h"
#include "detection/word/word_detection_pipeline.h"
#include "recipes/prepared_recipe.h"
#include "runtime/detection_worker.h"
#include "runtime/inspection_profile_snapshot.h"

#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class IBarcodeDecoder;
// 组件说明：IOcrEngine 组件提供对应设备或检测能力的统一实现。
class IOcrEngine;

// 组件说明：BarcodeRuntimeReadiness 数据结构集中保存该流程需要的一组相关数据。
struct BarcodeRuntimeReadiness
{
    bool ready = true;
    QString errorMessage;
};

// 组件说明：PipelineRegistryRequest 数据结构集中传递该流程需要的只读数据或回调。
struct PipelineRegistryRequest
{
    DetectionMode mode = DetectionMode::Stamp;
    PreparedRecipeSnapshot preparedRecipe;
    InspectionProfileSnapshot profileSnapshot;
};

// 组件说明：PipelineResultConsumers 数据结构集中保存该流程需要的一组相关数据。
struct PipelineResultConsumers
{
    typedef std::function<void(
        const DetectionCompletion &,
        const TissueRollResult &)> TissueConsumer;
    typedef std::function<void(
        const DetectionCompletion &,
        const DetectionPose &)> OcrConsumer;
    typedef std::function<void(
        const DetectionCompletion &,
        const StampDetectionWorkOutput &)> StampConsumer;
    typedef std::function<void(
        const DetectionCompletion &,
        const WordDetectionWorkOutput &)> WordConsumer;
    typedef std::function<void(
        const DetectionCompletion &,
        const BarcodeWordDetectionWorkOutput &)> BarcodeWordConsumer;

    TissueConsumer tissue;
    OcrConsumer ocr;
    StampConsumer stamp;
    WordConsumer word;
    BarcodeWordConsumer barcodeWord;
};

// 组件说明：PipelineCreationResult 数据结构保存一次操作的结果、状态和错误信息。
struct PipelineCreationResult
{
    std::shared_ptr<DetectionWorker> worker;
    QString errorMessage;
    QString startFailureMessage;
    QString workerLogName;

    // 函数说明：isAccepted 函数检查相关状态并返回判断结果。
    bool isAccepted() const
    {
        return worker && errorMessage.isEmpty();
    }
};

// 组件说明：PipelineRegistry 组件封装对应业务职责和生命周期边界。
class PipelineRegistry
{
public:
    PipelineRegistry(
        const std::shared_ptr<IOcrEngine> &ocrEngine,
        const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder);

    BarcodeRuntimeReadiness prepare(DetectionMode mode) const;
    PipelineCreationResult create(
        const PipelineRegistryRequest &request,
        const PipelineResultConsumers &consumers,
        const DetectionWorker::FailureConsumer &failureConsumer =
            DetectionWorker::FailureConsumer()) const;

private:
    // 组件说明：StampConfiguration 组件集中描述相关配置、规则和运行参数。
    struct StampConfiguration
    {
        QString targetText;
        PreparedCharacterTemplates preparedTemplates;
        std::vector<int> templateTargetIndexes;
        int thresholdPercent = 0;
        StampDetectionPipeline::OverlapDetectionFunction detectOverlap;
    };

    static std::shared_ptr<DetectionWorker> createTissueWorker(
        const TissueRecipeParameters &parameters,
        const PipelineResultConsumers::TissueConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer);
    static std::shared_ptr<DetectionWorker> createOcrWorker(
        const std::string &targetText,
        IOcrEngine *ocrEngine,
        const PipelineResultConsumers::OcrConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer);
    static std::shared_ptr<DetectionWorker> createStampWorker(
        const StampConfiguration &configuration,
        const PipelineResultConsumers::StampConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer);
    static std::shared_ptr<DetectionWorker> createWordWorker(
        const std::vector<DetectionModeWorkerProfile> &profiles,
        const PipelineResultConsumers::WordConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer);
    static std::shared_ptr<DetectionWorker> createBarcodeWordWorker(
        const std::vector<DetectionModeWorkerProfile> &profiles,
        IBarcodeDecoder *decoder,
        const PipelineResultConsumers::BarcodeWordConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer);

    bool buildStampConfiguration(
        const PreparedRecipe &prepared,
        StampConfiguration *configuration,
        QString *errorMessage) const;

    std::shared_ptr<IOcrEngine> m_ocrEngine;
    std::shared_ptr<IBarcodeDecoder> m_barcodeDecoder;
};
