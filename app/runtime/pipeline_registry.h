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
class IOcrEngine;

struct BarcodeRuntimeReadiness
{
    bool ready = true;
    QString errorMessage;
};

struct PipelineRegistryRequest
{
    DetectionMode mode = DetectionMode::Stamp;
    PreparedRecipeSnapshot preparedRecipe;
    InspectionProfileSnapshot profileSnapshot;
};

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

struct PipelineCreationResult
{
    std::shared_ptr<DetectionWorker> worker;
    QString errorMessage;
    QString startFailureMessage;
    QString workerLogName;

    bool isAccepted() const
    {
        return worker && errorMessage.isEmpty();
    }
};

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
