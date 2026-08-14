#pragma once

#include "BarcodeTypes.h"
#include "detection/barcode_word/barcode_word_detection_pipeline.h"
#include "detection/common/character_template_matcher.h"
#include "detection/stamp/stamp_detection_pipeline.h"
#include "detection/tissue/tissue_detection_pipeline.h"
#include "detection/word/word_detection_pipeline.h"
#include "runtime/detection_worker.h"

#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <vector>

class IBarcodeDecoder;
class IOcrEngine;

struct DetectionModeWorkerProfile
{
    QString templateName;
    QString targetText;
    TemplateMatchPreparedTemplates preparedTemplates;
    std::vector<int> templateTargetIndexes;
    int thresholdPercent = 0;
    BarcodeDecodeOptions barcodeOptions;
    BarcodeWordDecodeStrategyState decodeStrategy;
};

struct StampDetectionWorkerConfiguration
{
    QString targetText;
    TemplateMatchPreparedTemplates preparedTemplates;
    std::vector<int> templateTargetIndexes;
    int thresholdPercent = 0;
    StampDetectionPipeline::OverlapDetectionFunction detectOverlap;
};

class DetectionModeWorkerFactory
{
public:
    typedef std::function<void(
        const DetectionCompletion &,
        const TissueRollResult &)> TissueCompletionConsumer;
    typedef std::function<void(
        const DetectionCompletion &,
        const DetectionPose &)> OcrCompletionConsumer;
    typedef std::function<void(
        const DetectionCompletion &,
        const StampDetectionWorkOutput &)> StampCompletionConsumer;
    typedef std::function<void(
        const DetectionCompletion &,
        const WordDetectionWorkOutput &)> WordCompletionConsumer;
    typedef std::function<void(
        const DetectionCompletion &,
        const BarcodeWordDetectionWorkOutput &)>
        BarcodeWordCompletionConsumer;

    static std::shared_ptr<DetectionWorker> createTissueWorker(
        const TissueRecipeParameters &parameters,
        const TissueCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer =
            DetectionWorker::FailureConsumer());

    static std::shared_ptr<DetectionWorker> createOcrWorker(
        const std::string &targetText,
        IOcrEngine *ocrEngine,
        const OcrCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer =
            DetectionWorker::FailureConsumer());

    static std::shared_ptr<DetectionWorker> createStampWorker(
        const StampDetectionWorkerConfiguration &configuration,
        const StampCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer =
            DetectionWorker::FailureConsumer());

    static std::shared_ptr<DetectionWorker> createWordWorker(
        const std::vector<DetectionModeWorkerProfile> &profiles,
        const WordCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer =
            DetectionWorker::FailureConsumer());

    static std::shared_ptr<DetectionWorker> createBarcodeWordWorker(
        const std::vector<DetectionModeWorkerProfile> &profiles,
        IBarcodeDecoder *decoder,
        const BarcodeWordCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer =
            DetectionWorker::FailureConsumer());
};
