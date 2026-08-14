#include "runtime/detection_mode_worker_factory.h"

#include "detection/ocr/ocr_detection_pipeline.h"
#include "devices/barcode/barcode_decoder_adapter.h"
#include "devices/ocr/ocr_engine.h"

#include <chrono>

namespace {

const std::size_t kModeWorkerQueueCapacity = 1;

struct TissueDetectionState
{
    TissueRollResult lastResult;
};

struct OcrDetectionState
{
    DetectionPose lastPose;
};

struct StampDetectionState
{
    StampDetectionWorkOutput lastOutput;
};

struct WordDetectionState
{
    WordDetectionWorkOutput lastOutput;
};

struct BarcodeWordDetectionState
{
    BarcodeWordDetectionWorkOutput lastOutput;
};

} // namespace

std::shared_ptr<DetectionWorker>
DetectionModeWorkerFactory::createTissueWorker(
        const TissueRecipeParameters &parameters,
        const TissueCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (!completionConsumer) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<TissueDetectionPipeline> pipeline(
                new TissueDetectionPipeline(parameters));
    const std::shared_ptr<TissueDetectionState> state(
                new TissueDetectionState);
    return std::shared_ptr<DetectionWorker>(
                new DetectionWorker(
                    kModeWorkerQueueCapacity,
                    [pipeline, state](
                        const std::shared_ptr<const FrameData> &frame) {
        const std::chrono::high_resolution_clock::time_point start =
                std::chrono::high_resolution_clock::now();
        TissueRollResult tissueResult =
                pipeline->detect(frame->originalImage);
        tissueResult.processingTimeMs = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start)
                    .count());
        state->lastResult = tissueResult;
        return TissueDetectionPipeline::toDetectionResult(tissueResult);
    },
                    [state, completionConsumer](
                        const DetectionCompletion &completion) {
        completionConsumer(completion, state->lastResult);
    },
                    failureConsumer));
}

std::shared_ptr<DetectionWorker>
DetectionModeWorkerFactory::createOcrWorker(
        const std::string &targetText,
        IOcrEngine *ocrEngine,
        const OcrCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (!ocrEngine || !completionConsumer) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<OcrDetectionPipeline> pipeline(
                new OcrDetectionPipeline);
    const std::shared_ptr<OcrDetectionState> state(
                new OcrDetectionState);
    return std::shared_ptr<DetectionWorker>(
                new DetectionWorker(
                    kModeWorkerQueueCapacity,
                    DetectionWorker::WorkItemDetector(
                        [pipeline, state, targetText, ocrEngine](
                            const DetectionWorkItem &item) {
        state->lastPose = item.pose;
        return pipeline->detect(item, targetText, *ocrEngine);
    }),
                    [state, completionConsumer](
                        const DetectionCompletion &completion) {
        completionConsumer(completion, state->lastPose);
    },
                    failureConsumer));
}

std::shared_ptr<DetectionWorker>
DetectionModeWorkerFactory::createStampWorker(
        const StampDetectionWorkerConfiguration &configuration,
        const StampCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (!completionConsumer) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<StampDetectionPipeline> pipeline(
                new StampDetectionPipeline);
    const std::shared_ptr<StampDetectionState> state(
                new StampDetectionState);
    return std::shared_ptr<DetectionWorker>(
                new DetectionWorker(
                    kModeWorkerQueueCapacity,
                    DetectionWorker::WorkItemDetector(
                        [pipeline, state, configuration](
                            const DetectionWorkItem &item) {
        state->lastOutput = pipeline->detect(
                    item,
                    configuration.targetText,
                    configuration.preparedTemplates,
                    configuration.templateTargetIndexes,
                    configuration.thresholdPercent,
                    configuration.detectOverlap);
        return state->lastOutput.detectionResult;
    }),
                    [state, completionConsumer](
                        const DetectionCompletion &completion) {
        completionConsumer(completion, state->lastOutput);
    },
                    failureConsumer));
}

std::shared_ptr<DetectionWorker>
DetectionModeWorkerFactory::createWordWorker(
        const std::vector<DetectionModeWorkerProfile> &profiles,
        const WordCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (!completionConsumer) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<WordDetectionPipeline> pipeline(
                new WordDetectionPipeline);
    const std::shared_ptr<WordDetectionState> state(
                new WordDetectionState);
    const std::shared_ptr<const std::vector<DetectionModeWorkerProfile> >
            runtimeProfiles(
                new std::vector<DetectionModeWorkerProfile>(profiles));
    return std::shared_ptr<DetectionWorker>(
                new DetectionWorker(
                    kModeWorkerQueueCapacity,
                    DetectionWorker::WorkItemDetector(
                        [pipeline, state, runtimeProfiles](
                            const DetectionWorkItem &item) {
        if (!item.hasPose || !item.pose.valid) {
            state->lastOutput = pipeline->detect(
                        item,
                        QString(),
                        QString(),
                        TemplateMatchPreparedTemplates(),
                        std::vector<int>(),
                        0);
            return state->lastOutput.detectionResult;
        }

        const int profileIndex = item.pose.wordTemplateProfileIndex;
        if (profileIndex < 0
                || profileIndex
                >= static_cast<int>(runtimeProfiles->size())) {
            state->lastOutput = WordDetectionWorkOutput();
            state->lastOutput.pose = item.pose;
            state->lastOutput.detectionResult.modeId =
                    QStringLiteral("word_detection");
            state->lastOutput.detectionResult.status =
                    DetectionStatus::Cancelled;
            state->lastOutput.detectionResult.diagnostic =
                    QStringLiteral("Invalid word profile index");
            return state->lastOutput.detectionResult;
        }

        const DetectionModeWorkerProfile &profile =
                runtimeProfiles->at(
                    static_cast<std::size_t>(profileIndex));
        state->lastOutput = pipeline->detect(
                    item,
                    profile.targetText,
                    profile.templateName,
                    profile.preparedTemplates,
                    profile.templateTargetIndexes,
                    profile.thresholdPercent);
        return state->lastOutput.detectionResult;
    }),
                    [state, completionConsumer](
                        const DetectionCompletion &completion) {
        completionConsumer(completion, state->lastOutput);
    },
                    failureConsumer));
}

std::shared_ptr<DetectionWorker>
DetectionModeWorkerFactory::createBarcodeWordWorker(
        const std::vector<DetectionModeWorkerProfile> &profiles,
        IBarcodeDecoder *decoder,
        const BarcodeWordCompletionConsumer &completionConsumer,
        const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (!decoder || !completionConsumer) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<BarcodeWordDetectionPipeline> pipeline(
                new BarcodeWordDetectionPipeline);
    const std::shared_ptr<BarcodeWordDetectionState> state(
                new BarcodeWordDetectionState);
    const std::shared_ptr<std::vector<DetectionModeWorkerProfile> >
            runtimeProfiles(
                new std::vector<DetectionModeWorkerProfile>(profiles));
    return std::shared_ptr<DetectionWorker>(
                new DetectionWorker(
                    kModeWorkerQueueCapacity,
                    DetectionWorker::WorkItemDetector(
                        [pipeline, state, runtimeProfiles, decoder](
                            const DetectionWorkItem &item) {
        if (!item.hasPose || !item.pose.valid) {
            state->lastOutput = pipeline->detect(
                        item,
                        QString(),
                        QString(),
                        TemplateMatchPreparedTemplates(),
                        std::vector<int>(),
                        0,
                        BarcodeDecodeOptions(),
                        BarcodeWordDecodeStrategyState(),
                        decoder);
            return state->lastOutput.detectionResult;
        }

        const int profileIndex = item.pose.wordTemplateProfileIndex;
        if (profileIndex < 0
                || profileIndex
                >= static_cast<int>(runtimeProfiles->size())) {
            state->lastOutput = BarcodeWordDetectionWorkOutput();
            state->lastOutput.pose = item.pose;
            state->lastOutput.detectionResult.modeId =
                    QStringLiteral("barcode_word_detection");
            state->lastOutput.detectionResult.status =
                    DetectionStatus::Cancelled;
            state->lastOutput.detectionResult.diagnostic =
                    QStringLiteral("Invalid barcode-word profile index");
            return state->lastOutput.detectionResult;
        }

        DetectionModeWorkerProfile &profile =
                runtimeProfiles->at(
                    static_cast<std::size_t>(profileIndex));
        state->lastOutput = pipeline->detect(
                    item,
                    profile.targetText,
                    profile.templateName,
                    profile.preparedTemplates,
                    profile.templateTargetIndexes,
                    profile.thresholdPercent,
                    profile.barcodeOptions,
                    profile.decodeStrategy,
                    decoder);
        profile.decodeStrategy = state->lastOutput.nextDecodeStrategy;
        return state->lastOutput.detectionResult;
    }),
                    [state, completionConsumer](
                        const DetectionCompletion &completion) {
        completionConsumer(completion, state->lastOutput);
    },
                    failureConsumer));
}

DetectionModeWorkerCreationResult DetectionModeWorkerDispatcher::create(
        const DetectionModeWorkerRequest &request,
        const DetectionModeWorkerConsumers &consumers,
        const DetectionWorker::FailureConsumer &failureConsumer)
{
    DetectionModeWorkerCreationResult result;
    switch (request.modeIndex) {
    case 0:
        result.workerLogName = QStringLiteral("stamp");
        result.startFailureMessage = QStringLiteral(
                    "\u65e0\u6cd5\u542f\u52a8\u94a2\u5370\u68c0\u6d4b"
                    "\u5de5\u4f5c\u7ebf\u7a0b\u3002");
        if (!request.stampConfiguration.preparedTemplates.isValid()) {
            result.errorMessage = QStringLiteral(
                        "\u94a2\u5370\u5b57\u7b26\u6a21\u677f\u6216"
                        "\u56fe\u50cf\u9608\u503c\u65e0\u6548\u3002");
            return result;
        }
        result.worker = DetectionModeWorkerFactory::createStampWorker(
                    request.stampConfiguration,
                    consumers.stamp,
                    failureConsumer);
        break;
    case 1:
        result.workerLogName = QStringLiteral("word");
        result.startFailureMessage = QStringLiteral(
                    "\u65e0\u6cd5\u542f\u52a8\u5b57\u5e93\u68c0\u6d4b"
                    "\u5de5\u4f5c\u7ebf\u7a0b\u3002");
        if (request.profiles.empty()) {
            result.errorMessage = QStringLiteral(
                        "\u5b57\u5e93\u8fd0\u884c Profile "
                        "\u5feb\u7167\u672a\u51c6\u5907\u3002");
            return result;
        }
        result.worker = DetectionModeWorkerFactory::createWordWorker(
                    request.profiles,
                    consumers.word,
                    failureConsumer);
        break;
    case 2:
        result.workerLogName = QStringLiteral("OCR");
        result.startFailureMessage = QStringLiteral(
                    "\u65e0\u6cd5\u542f\u52a8\u6df1\u5ea6 OCR "
                    "\u68c0\u6d4b\u5de5\u4f5c\u7ebf\u7a0b\u3002");
        if (!request.ocrEngine) {
            result.errorMessage = QStringLiteral(
                        "\u6df1\u5ea6 OCR \u5f15\u64ce\u672a\u521d\u59cb\u5316\u3002");
            return result;
        }
        result.worker = DetectionModeWorkerFactory::createOcrWorker(
                    request.targetText,
                    request.ocrEngine,
                    consumers.ocr,
                    failureConsumer);
        break;
    case 3:
        result.workerLogName = QStringLiteral("tissue");
        result.startFailureMessage = QStringLiteral(
                    "\u65e0\u6cd5\u542f\u52a8\u7eb8\u5dfe\u68c0\u6d4b"
                    "\u5de5\u4f5c\u7ebf\u7a0b\u3002");
        result.worker = DetectionModeWorkerFactory::createTissueWorker(
                    request.tissueParameters,
                    consumers.tissue,
                    failureConsumer);
        break;
    case 4:
        result.workerLogName = QStringLiteral("barcode-word");
        result.startFailureMessage = QStringLiteral(
                    "\u65e0\u6cd5\u542f\u52a8\u4e8c\u7ef4\u7801+"
                    "\u4e09\u671f\u68c0\u6d4b\u5de5\u4f5c\u7ebf\u7a0b\u3002");
        if (request.profiles.empty()) {
            result.errorMessage = QStringLiteral(
                        "\u4e8c\u7ef4\u7801+\u4e09\u671f\u8fd0\u884c "
                        "Profile \u5feb\u7167\u672a\u51c6\u5907\u3002");
            return result;
        }
        if (!request.barcodeDecoder) {
            result.errorMessage = QStringLiteral(
                        "Barcode decoder is null");
            return result;
        }
        if (!request.barcodeDecoder->ensureLoaded()) {
            result.errorMessage = request.barcodeDecoder->lastError();
            return result;
        }
        result.worker =
                DetectionModeWorkerFactory::createBarcodeWordWorker(
                    request.profiles,
                    request.barcodeDecoder,
                    consumers.barcodeWord,
                    failureConsumer);
        break;
    default:
        result.errorMessage = QStringLiteral(
                    "\u4e0d\u652f\u6301\u7684\u68c0\u6d4b\u6a21\u5f0f\u3002");
        return result;
    }

    if (!result.worker) {
        result.errorMessage = result.startFailureMessage;
    }
    return result;
}
