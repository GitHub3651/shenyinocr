#include "runtime/pipeline_registry.h"

#include "Detector.h"
#include "detection/ocr/ocr_detection_pipeline.h"
#include "devices/barcode/barcode_decoder.h"
#include "devices/ocr/ocr_engine.h"

#include <chrono>

namespace {

const std::size_t kPipelineQueueCapacity = 1;

struct TissueState
{
    TissueRollResult output;
};

struct OcrState
{
    DetectionPose pose;
};

struct StampState
{
    StampDetectionWorkOutput output;
};

struct WordState
{
    WordDetectionWorkOutput output;
};

struct BarcodeWordState
{
    BarcodeWordDetectionWorkOutput output;
};

} // namespace

PipelineRegistry::PipelineRegistry(
    const std::shared_ptr<IOcrEngine> &ocrEngine,
    const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder)
    : m_ocrEngine(ocrEngine),
      m_barcodeDecoder(barcodeDecoder)
{
}

BarcodeRuntimeReadiness PipelineRegistry::prepare(DetectionMode mode) const
{
    BarcodeRuntimeReadiness readiness;
    if (mode != DetectionMode::BarcodeWord) {
        return readiness;
    }

    readiness.ready = m_barcodeDecoder
            && m_barcodeDecoder->ensureLoaded();
    if (!readiness.ready && m_barcodeDecoder) {
        readiness.errorMessage = m_barcodeDecoder->lastError();
    }
    if (!m_barcodeDecoder) {
        readiness.errorMessage = QStringLiteral(
                    "Barcode decoder is not available.");
    }
    return readiness;
}

std::shared_ptr<DetectionWorker> PipelineRegistry::createTissueWorker(
    const TissueRecipeParameters &parameters,
    const PipelineResultConsumers::TissueConsumer &completionConsumer,
    const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (!completionConsumer) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<TissueDetectionPipeline> pipeline(
                new TissueDetectionPipeline(parameters));
    const std::shared_ptr<TissueState> state(new TissueState);
    return std::shared_ptr<DetectionWorker>(new DetectionWorker(
        kPipelineQueueCapacity,
        [pipeline, state](const std::shared_ptr<const FrameData> &frame) {
            const std::chrono::high_resolution_clock::time_point start =
                    std::chrono::high_resolution_clock::now();
            state->output = pipeline->detect(frame->originalImage);
            state->output.processingTimeMs = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::high_resolution_clock::now() - start)
                        .count());
            return TissueDetectionPipeline::toDetectionResult(state->output);
        },
        [state, completionConsumer](const DetectionCompletion &completion) {
            completionConsumer(completion, state->output);
        },
        failureConsumer));
}

std::shared_ptr<DetectionWorker> PipelineRegistry::createOcrWorker(
    const std::string &targetText,
    IOcrEngine *ocrEngine,
    const PipelineResultConsumers::OcrConsumer &completionConsumer,
    const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (!ocrEngine || !completionConsumer) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<OcrDetectionPipeline> pipeline(
                new OcrDetectionPipeline);
    const std::shared_ptr<OcrState> state(new OcrState);
    return std::shared_ptr<DetectionWorker>(new DetectionWorker(
        kPipelineQueueCapacity,
        DetectionWorker::WorkItemDetector(
            [pipeline, state, targetText, ocrEngine](
                const DetectionWorkItem &item) {
                state->pose = item.pose;
                return pipeline->detect(item, targetText, *ocrEngine);
            }),
        [state, completionConsumer](const DetectionCompletion &completion) {
            completionConsumer(completion, state->pose);
        },
        failureConsumer));
}

std::shared_ptr<DetectionWorker> PipelineRegistry::createStampWorker(
    const StampConfiguration &configuration,
    const PipelineResultConsumers::StampConsumer &completionConsumer,
    const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (!completionConsumer
            || !configuration.preparedTemplates.isValid()) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<StampDetectionPipeline> pipeline(
                new StampDetectionPipeline);
    const std::shared_ptr<StampState> state(new StampState);
    return std::shared_ptr<DetectionWorker>(new DetectionWorker(
        kPipelineQueueCapacity,
        DetectionWorker::WorkItemDetector(
            [pipeline, state, configuration](
                const DetectionWorkItem &item) {
                state->output = pipeline->detect(
                            item,
                            configuration.targetText,
                            configuration.preparedTemplates,
                            configuration.templateTargetIndexes,
                            configuration.thresholdPercent,
                            configuration.detectOverlap);
                return state->output.detectionResult;
            }),
        [state, completionConsumer](const DetectionCompletion &completion) {
            completionConsumer(completion, state->output);
        },
        failureConsumer));
}

std::shared_ptr<DetectionWorker> PipelineRegistry::createWordWorker(
    const std::vector<DetectionModeWorkerProfile> &profiles,
    const PipelineResultConsumers::WordConsumer &completionConsumer,
    const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (profiles.empty() || !completionConsumer) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<WordDetectionPipeline> pipeline(
                new WordDetectionPipeline);
    const std::shared_ptr<WordState> state(new WordState);
    const std::shared_ptr<const std::vector<DetectionModeWorkerProfile> >
            runtimeProfiles(new std::vector<DetectionModeWorkerProfile>(profiles));
    return std::shared_ptr<DetectionWorker>(new DetectionWorker(
        kPipelineQueueCapacity,
        DetectionWorker::WorkItemDetector(
            [pipeline, state, runtimeProfiles](
                const DetectionWorkItem &item) {
                if (!item.hasPose || !item.pose.valid) {
                    state->output = pipeline->detect(
                                item,
                                QString(),
                                QString(),
                                TemplateMatchPreparedTemplates(),
                                std::vector<int>(),
                                0);
                    return state->output.detectionResult;
                }

                const int profileIndex = item.pose.wordTemplateProfileIndex;
                if (profileIndex < 0
                        || profileIndex >= static_cast<int>(
                            runtimeProfiles->size())) {
                    state->output = WordDetectionWorkOutput();
                    state->output.pose = item.pose;
                    state->output.detectionResult.modeId =
                            QStringLiteral("word_detection");
                    state->output.detectionResult.status =
                            DetectionStatus::Cancelled;
                    state->output.detectionResult.diagnostic =
                            QStringLiteral("Invalid word profile index");
                    return state->output.detectionResult;
                }

                const DetectionModeWorkerProfile &profile =
                        runtimeProfiles->at(
                            static_cast<std::size_t>(profileIndex));
                state->output = pipeline->detect(
                            item,
                            profile.targetText,
                            profile.templateName,
                            profile.preparedTemplates,
                            profile.templateTargetIndexes,
                            profile.thresholdPercent);
                return state->output.detectionResult;
            }),
        [state, completionConsumer](const DetectionCompletion &completion) {
            completionConsumer(completion, state->output);
        },
        failureConsumer));
}

std::shared_ptr<DetectionWorker> PipelineRegistry::createBarcodeWordWorker(
    const std::vector<DetectionModeWorkerProfile> &profiles,
    IBarcodeDecoder *decoder,
    const PipelineResultConsumers::BarcodeWordConsumer &completionConsumer,
    const DetectionWorker::FailureConsumer &failureConsumer)
{
    if (profiles.empty() || !decoder || !completionConsumer) {
        return std::shared_ptr<DetectionWorker>();
    }

    const std::shared_ptr<BarcodeWordDetectionPipeline> pipeline(
                new BarcodeWordDetectionPipeline);
    const std::shared_ptr<BarcodeWordState> state(new BarcodeWordState);
    const std::shared_ptr<std::vector<DetectionModeWorkerProfile> >
            runtimeProfiles(new std::vector<DetectionModeWorkerProfile>(profiles));
    return std::shared_ptr<DetectionWorker>(new DetectionWorker(
        kPipelineQueueCapacity,
        DetectionWorker::WorkItemDetector(
            [pipeline, state, runtimeProfiles, decoder](
                const DetectionWorkItem &item) {
                if (!item.hasPose || !item.pose.valid) {
                    state->output = pipeline->detect(
                                item,
                                QString(),
                                QString(),
                                TemplateMatchPreparedTemplates(),
                                std::vector<int>(),
                                0,
                                BarcodeDecodeOptions(),
                                BarcodeWordDecodeStrategyState(),
                                decoder);
                    return state->output.detectionResult;
                }

                const int profileIndex = item.pose.wordTemplateProfileIndex;
                if (profileIndex < 0
                        || profileIndex >= static_cast<int>(
                            runtimeProfiles->size())) {
                    state->output = BarcodeWordDetectionWorkOutput();
                    state->output.pose = item.pose;
                    state->output.detectionResult.modeId =
                            QStringLiteral("barcode_word_detection");
                    state->output.detectionResult.status =
                            DetectionStatus::Cancelled;
                    state->output.detectionResult.diagnostic =
                            QStringLiteral("Invalid barcode-word profile index");
                    return state->output.detectionResult;
                }

                DetectionModeWorkerProfile &profile = runtimeProfiles->at(
                            static_cast<std::size_t>(profileIndex));
                state->output = pipeline->detect(
                            item,
                            profile.targetText,
                            profile.templateName,
                            profile.preparedTemplates,
                            profile.templateTargetIndexes,
                            profile.thresholdPercent,
                            profile.barcodeOptions,
                            profile.decodeStrategy,
                            decoder);
                profile.decodeStrategy = state->output.nextDecodeStrategy;
                return state->output.detectionResult;
            }),
        [state, completionConsumer](const DetectionCompletion &completion) {
            completionConsumer(completion, state->output);
        },
        failureConsumer));
}

bool PipelineRegistry::buildStampConfiguration(
    const PreparedRecipe &prepared,
    StampConfiguration *configuration,
    QString *errorMessage) const
{
    if (!configuration || prepared.profiles.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "钢印运行配方缺少Prepared Profile。");
        }
        return false;
    }

    const PreparedRecipeProfile &profile = prepared.profiles.first();
    configuration->targetText = profile.definition.targetText;
    configuration->thresholdPercent =
            profile.definition.imageThresholdPercent;
    configuration->preparedTemplates = CharacterTemplateMatcher::prepare(
                profile.characterTemplates);
    configuration->templateTargetIndexes =
            profile.characterTemplateTargetIndexes;
    if (!configuration->preparedTemplates.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "钢印字符模板或图像阈值无效。");
        }
        return false;
    }

    const std::shared_ptr<OverlapDetector> overlap(new OverlapDetector);
    CalibrationData calibration;
    calibration.stamp_poly = profile.stampPolygon;
    calibration.date_poly = profile.datePolygon;
    calibration.barcode_poly = profile.barcodePolygon;
    if (!overlap->init(profile.stampRingTemplate, calibration)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "钢印PreparedRecipe无法初始化防重叠检测资产。");
        }
        return false;
    }
    configuration->detectOverlap = [overlap](
        const cv::Mat &sourceImage,
        const std::vector<cv::Point> &datePoly) {
        const DetectResult detected = overlap->processImage(
                    sourceImage, datePoly);
        StampOverlapResult result;
        result.isOk = detected.isOk;
        result.finalStampPoly = detected.finalStampPoly;
        return result;
    };
    return true;
}

PipelineCreationResult PipelineRegistry::create(
    const PipelineRegistryRequest &request,
    const PipelineResultConsumers &consumers,
    const DetectionWorker::FailureConsumer &failureConsumer) const
{
    PipelineCreationResult result;
    if (!request.preparedRecipe || !request.preparedRecipe->recipe) {
        result.errorMessage = QStringLiteral("运行配方快照不可用。");
        return result;
    }
    if (request.preparedRecipe->recipe->detectionMode != request.mode) {
        result.errorMessage = QStringLiteral("运行配方模式不匹配。");
        return result;
    }

    const PreparedRecipe &prepared = *request.preparedRecipe;
    switch (request.mode) {
    case DetectionMode::Stamp: {
        result.workerLogName = QStringLiteral("stamp");
        result.startFailureMessage = QStringLiteral(
                    "无法启动钢印检测工作线程。");
        StampConfiguration configuration;
        if (!buildStampConfiguration(
                    prepared, &configuration, &result.errorMessage)) {
            return result;
        }
        result.worker = createStampWorker(
                    configuration, consumers.stamp, failureConsumer);
        break;
    }
    case DetectionMode::Word:
        result.workerLogName = QStringLiteral("word");
        result.startFailureMessage = QStringLiteral(
                    "无法启动字库检测工作线程。");
        if (!request.profileSnapshot.isValid()) {
            result.errorMessage = QStringLiteral(
                        "字库运行Profile快照未准备。");
            return result;
        }
        result.worker = createWordWorker(
                    request.profileSnapshot.detectionProfiles,
                    consumers.word,
                    failureConsumer);
        break;
    case DetectionMode::Ocr:
        result.workerLogName = QStringLiteral("OCR");
        result.startFailureMessage = QStringLiteral(
                    "无法启动深度OCR检测工作线程。");
        if (!m_ocrEngine || prepared.profiles.isEmpty()) {
            result.errorMessage = QStringLiteral(
                        "深度OCR引擎或目标文本未初始化。");
            return result;
        }
        result.worker = createOcrWorker(
                    prepared.profiles.first().definition.targetText.toStdString(),
                    m_ocrEngine.get(),
                    consumers.ocr,
                    failureConsumer);
        break;
    case DetectionMode::Tissue:
        result.workerLogName = QStringLiteral("tissue");
        result.startFailureMessage = QStringLiteral(
                    "无法启动纸巾检测工作线程。");
        result.worker = createTissueWorker(
                    prepared.tissue, consumers.tissue, failureConsumer);
        break;
    case DetectionMode::BarcodeWord:
        result.workerLogName = QStringLiteral("barcode-word");
        result.startFailureMessage = QStringLiteral(
                    "无法启动二维码+三期检测工作线程。");
        if (!request.profileSnapshot.isValid()) {
            result.errorMessage = QStringLiteral(
                        "二维码+三期运行Profile快照未准备。");
            return result;
        }
        if (!prepare(DetectionMode::BarcodeWord).ready) {
            result.errorMessage = m_barcodeDecoder
                    ? m_barcodeDecoder->lastError()
                    : QStringLiteral("Barcode decoder is not available.");
            return result;
        }
        result.worker = createBarcodeWordWorker(
                    request.profileSnapshot.detectionProfiles,
                    m_barcodeDecoder.get(),
                    consumers.barcodeWord,
                    failureConsumer);
        break;
    }

    if (!result.worker) {
        result.errorMessage = result.startFailureMessage;
    }
    return result;
}
