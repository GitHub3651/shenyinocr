// 文件作用：本文件在Detection内完成预处理、定位和模式算法装配。
#include "detection/detection_registry.h"

#include "detection/barcode_word/barcode_word_detection_pipeline.h"
#include "detection/common/character_template_matcher.h"
#include "detection/ocr/ocr_detection_pipeline.h"
#include "detection/positioning/inspection_positioner.h"
#include "detection/stamp/overlap_detector.h"
#include "detection/stamp/stamp_detection_pipeline.h"
#include "detection/tissue/tissue_detection_pipeline.h"
#include "detection/word/word_detection_pipeline.h"
#include "engines/barcode/barcode_decoder.h"
#include "engines/ocr/ocr_engine.h"

#include <QStringList>
#include <chrono>
#include <string>

namespace {

struct StampConfiguration
{
    QString targetText;
    PreparedCharacterTemplates preparedTemplates;
    std::vector<int> templateTargetIndexes;
    int thresholdPercent = 0;
    StampDetectionPipeline::OverlapDetectionFunction detectOverlap;
};

bool buildStampConfiguration(
    const PreparedRecipe &prepared,
    StampConfiguration *configuration,
    QString *errorMessage)
{
    if (!configuration || prepared.profiles.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("钢印运行配方缺少Prepared Profile。");
        }
        return false;
    }
    const PreparedRecipeProfile &profile = prepared.profiles.first();
    configuration->targetText = profile.definition.targetText;
    configuration->thresholdPercent = profile.definition.imageThresholdPercent;
    configuration->preparedTemplates = CharacterGlyphMatcher::prepare(
                profile.characterTemplates);
    configuration->templateTargetIndexes = profile.characterTemplateTargetIndexes;
    if (!configuration->preparedTemplates.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("钢印字符模板或图像阈值无效。");
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
        const DetectResult detected = overlap->processImage(sourceImage, datePoly);
        StampOverlapResult result;
        result.isOk = detected.isOk;
        result.finalStampPoly = detected.finalStampPoly;
        return result;
    };
    return true;
}

void applyDescriptorPolicy(
    const DetectionModeDescriptor &descriptor,
    DetectionResult *result)
{
    if (!result) return;
    result->modeId = QLatin1String(descriptor.uiId);
    result->clearImageLabelRects = descriptor.clearImageLabelRects;
    result->showRoiWarningOnCancelled = descriptor.showRoiWarningOnCancelled;
    result->clearRoiWarningOnCompleted = descriptor.showRoiWarningOnCancelled;
    result->saveRawOnly = descriptor.saveRawOnly;
    result->saveNotEvaluatedAsNg = descriptor.saveNotEvaluatedAsNg;
    result->elapsedDecimals = descriptor.elapsedDecimals;
}

void setProfilePresentation(
    const QString &templateName,
    const DetectionPose &pose,
    DetectionResult *result)
{
    if (!result) return;
    if (!templateName.trimmed().isEmpty()) {
        result->updatesTemplateName = true;
        result->templateName = templateName;
    } else if (!pose.valid) {
        result->updatesTemplateName = true;
        result->templateName = QStringLiteral("--");
    }
}

DetectionCompletion completeWith(
    const std::shared_ptr<const FrameData> &frame,
    const DetectionResult &result)
{
    DetectionCompletion completion;
    completion.frame = frame;
    completion.result = result;
    return completion;
}

std::shared_ptr<const FrameData> preprocessFrame(
    const std::shared_ptr<const FrameData> &source,
    const FramePreprocessSettings &settings)
{
    if (!source) return std::shared_ptr<const FrameData>();
    cv::Mat image;
    if (!FramePreprocessor::transform(source->originalImage, settings, &image)) {
        return std::shared_ptr<const FrameData>();
    }
    return makeFrameData(source->productKey, source->frameNumber,
                         source->cameraIndex, source->timestampUtc, image);
}

} // namespace

DetectionRegistry::DetectionRegistry(
    const std::shared_ptr<IOcrEngine> &ocrEngine,
    const std::shared_ptr<IBarcodeDecoder> &barcodeDecoder)
    : m_ocrEngine(ocrEngine),
      m_barcodeDecoder(barcodeDecoder)
{
}

DetectionRuntimeReadiness DetectionRegistry::prepare(DetectionMode mode) const
{
    DetectionRuntimeReadiness readiness;
    if (!detectionModeDescriptor(mode).requiresBarcodeDecoder) return readiness;
    readiness.ready = m_barcodeDecoder && m_barcodeDecoder->ensureLoaded();
    if (!readiness.ready) {
        readiness.errorMessage = m_barcodeDecoder
                ? m_barcodeDecoder->lastError()
                : QStringLiteral("Barcode decoder is not available.");
    }
    return readiness;
}

DetectionPipelineCreationResult DetectionRegistry::create(
    const DetectionRegistryRequest &request) const
{
    DetectionPipelineCreationResult creation;
    if (!request.preparedRecipe || !request.preparedRecipe->recipe) {
        creation.errorMessage = QStringLiteral("运行配方快照不可用。");
        return creation;
    }
    if (request.preparedRecipe->recipe->detectionMode != request.mode) {
        creation.errorMessage = QStringLiteral("运行配方模式不匹配。");
        return creation;
    }

    const DetectionModeDescriptor descriptor = detectionModeDescriptor(request.mode);
    const PreparedRecipe &prepared = *request.preparedRecipe;
    const PreparedRecipeProfile *first = prepared.profiles.isEmpty()
            ? nullptr : &prepared.profiles.first();

    StampConfiguration stampConfiguration;
    if (request.mode == DetectionMode::Stamp
            && !buildStampConfiguration(
                prepared, &stampConfiguration, &creation.errorMessage)) {
        return creation;
    }
    if (request.mode == DetectionMode::Ocr
            && (!m_ocrEngine || !first)) {
        creation.errorMessage = QStringLiteral(
                    "深度OCR引擎或目标文本未初始化。");
        return creation;
    }
    if (descriptor.trackingKind == DetectionTrackingKind::MultipleProfiles
            && !request.profileSnapshot.isValid()) {
        creation.errorMessage = request.mode == DetectionMode::BarcodeWord
                ? QStringLiteral("二维码+三期运行Profile快照未准备。")
                : QStringLiteral("字库运行Profile快照未准备。");
        return creation;
    }
    if (request.mode == DetectionMode::BarcodeWord
            && !prepare(DetectionMode::BarcodeWord).ready) {
        creation.errorMessage = m_barcodeDecoder
                ? m_barcodeDecoder->lastError()
                : QStringLiteral("Barcode decoder is not available.");
        return creation;
    }

    const std::shared_ptr<InspectionPositioner> positioner(new InspectionPositioner);
    if (!positioner->configure(
                descriptor.trackingKind,
                request.profileSnapshot.trackingProfiles,
                first ? first->datePolygon : std::vector<cv::Point2f>(),
                first ? first->trackingTemplate : cv::Mat())) {
        creation.errorMessage = QStringLiteral("运行定位资源初始化失败。");
        return creation;
    }

    creation.workerLogName = QString::fromUtf8(descriptor.workerLogName);
    creation.startFailureMessage = QString::fromUtf8(
                descriptor.startFailureMessage);
    const FramePreprocessSettings preprocess = request.framePreprocess;

    if (request.mode == DetectionMode::Tissue) {
        const std::shared_ptr<TissueDetectionPipeline> pipeline(
                    new TissueDetectionPipeline(prepared.tissue));
        creation.executor = [pipeline, preprocess, descriptor](
            const std::shared_ptr<const FrameData> &source) {
            const std::shared_ptr<const FrameData> frame = preprocessFrame(source, preprocess);
            if (!frame) return DetectionCompletion();
            const auto started = std::chrono::high_resolution_clock::now();
            TissueRollResult output = pipeline->detect(frame->originalImage);
            output.processingTimeMs = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::high_resolution_clock::now() - started).count());
            DetectionResult result = TissueDetectionPipeline::toDetectionResult(output);
            applyDescriptorPolicy(descriptor, &result);
            return completeWith(frame, result);
        };
        return creation;
    }

    if (request.mode == DetectionMode::Ocr) {
        const std::shared_ptr<OcrDetectionPipeline> pipeline(new OcrDetectionPipeline);
        IOcrEngine *engine = m_ocrEngine.get();
        const std::string target = first->definition.targetText.toStdString();
        creation.executor = [pipeline, positioner, preprocess, descriptor, engine, target](
            const std::shared_ptr<const FrameData> &source) {
            const std::shared_ptr<const FrameData> frame = preprocessFrame(source, preprocess);
            if (!frame) return DetectionCompletion();
            const DetectionPose pose = positioner->locate(frame->originalImage);
            DetectionResult result = pipeline->detect(
                        makeDetectionWorkItem(frame, pose), target, *engine);
            result.presentationText = result.recognizedText;
            result.hasPresentationText = true;
            applyDescriptorPolicy(descriptor, &result);
            return completeWith(frame, result);
        };
        return creation;
    }

    if (request.mode == DetectionMode::Stamp) {
        const std::shared_ptr<StampDetectionPipeline> pipeline(new StampDetectionPipeline);
        creation.executor = [pipeline, positioner, preprocess, descriptor, stampConfiguration](
            const std::shared_ptr<const FrameData> &source) {
            const std::shared_ptr<const FrameData> frame = preprocessFrame(source, preprocess);
            if (!frame) return DetectionCompletion();
            const DetectionPose pose = positioner->locate(frame->originalImage);
            StampDetectionWorkOutput output = pipeline->detect(
                        makeDetectionWorkItem(frame, pose), stampConfiguration.targetText,
                        stampConfiguration.preparedTemplates,
                        stampConfiguration.templateTargetIndexes,
                        stampConfiguration.thresholdPercent,
                        stampConfiguration.detectOverlap);
            DetectionResult result = output.detectionResult;
            if (output.hasOverlapDetection && !output.stampResult.overlapIsOk) {
                for (DetectionOverlayPolygon &polygon : result.overlay.polygons) {
                    if (polygon.role == QLatin1String("stamp")) polygon.alarm = true;
                }
            }
            applyDescriptorPolicy(descriptor, &result);
            return completeWith(frame, result);
        };
        return creation;
    }

    const std::shared_ptr<std::vector<DetectionModeWorkerProfile> > profiles(
                new std::vector<DetectionModeWorkerProfile>(
                    request.profileSnapshot.detectionProfiles));
    if (request.mode == DetectionMode::Word) {
        const std::shared_ptr<WordDetectionPipeline> pipeline(new WordDetectionPipeline);
        creation.executor = [pipeline, positioner, preprocess, descriptor, profiles](
            const std::shared_ptr<const FrameData> &source) {
            const std::shared_ptr<const FrameData> frame = preprocessFrame(source, preprocess);
            if (!frame) return DetectionCompletion();
            const DetectionPose pose = positioner->locate(frame->originalImage);
            const DetectionWorkItem item = makeDetectionWorkItem(frame, pose);
            WordDetectionWorkOutput output;
            if (!pose.valid) {
                output = pipeline->detect(item, QString(), QString(),
                                          PreparedCharacterTemplates(),
                                          std::vector<int>(), 0);
            } else if (pose.wordTemplateProfileIndex >= 0
                       && pose.wordTemplateProfileIndex
                          < static_cast<int>(profiles->size())) {
                const DetectionModeWorkerProfile &profile = profiles->at(
                            static_cast<std::size_t>(pose.wordTemplateProfileIndex));
                output = pipeline->detect(item, profile.targetText, profile.templateName,
                                          profile.preparedTemplates,
                                          profile.templateTargetIndexes,
                                          profile.thresholdPercent);
            } else {
                output.pose = pose;
                output.detectionResult.modeId = detectionModeUiId(
                            DetectionMode::Word);
                output.detectionResult.status = DetectionStatus::Cancelled;
                output.detectionResult.diagnostic = QStringLiteral(
                            "Invalid word profile index");
            }
            DetectionResult result = output.detectionResult;
            setProfilePresentation(output.templateName, output.pose, &result);
            applyDescriptorPolicy(descriptor, &result);
            return completeWith(frame, result);
        };
        return creation;
    }

    const std::shared_ptr<BarcodeWordDetectionPipeline> pipeline(
                new BarcodeWordDetectionPipeline);
    IBarcodeDecoder *decoder = m_barcodeDecoder.get();
    creation.executor = [pipeline, positioner, preprocess, descriptor, profiles, decoder](
        const std::shared_ptr<const FrameData> &source) {
        const std::shared_ptr<const FrameData> frame = preprocessFrame(source, preprocess);
        if (!frame) return DetectionCompletion();
        const DetectionPose pose = positioner->locate(frame->originalImage);
        const DetectionWorkItem item = makeDetectionWorkItem(frame, pose);
        BarcodeWordDetectionWorkOutput output;
        if (!pose.valid) {
            output = pipeline->detect(item, QString(), QString(),
                                      PreparedCharacterTemplates(),
                                      std::vector<int>(), 0,
                                      BarcodeDecodeOptions(),
                                      BarcodeWordDecodeStrategyState(), decoder);
        } else if (pose.wordTemplateProfileIndex >= 0
                   && pose.wordTemplateProfileIndex
                      < static_cast<int>(profiles->size())) {
            DetectionModeWorkerProfile &profile = profiles->at(
                        static_cast<std::size_t>(pose.wordTemplateProfileIndex));
            output = pipeline->detect(item, profile.targetText, profile.templateName,
                                      profile.preparedTemplates,
                                      profile.templateTargetIndexes,
                                      profile.thresholdPercent,
                                      profile.barcodeOptions,
                                      profile.decodeStrategy, decoder);
            profile.decodeStrategy = output.nextDecodeStrategy;
        } else {
            output.pose = pose;
            output.detectionResult.modeId = detectionModeUiId(
                        DetectionMode::BarcodeWord);
            output.detectionResult.status = DetectionStatus::Cancelled;
            output.detectionResult.diagnostic = QStringLiteral(
                        "Invalid barcode-word profile index");
        }
        DetectionResult result = output.detectionResult;
        setProfilePresentation(output.templateName, output.pose, &result);
        QStringList lines;
        lines.append(QStringLiteral("二维码：%1").arg(output.barcodeState));
        if (!output.barcode.text.isEmpty()) {
            lines.append(QStringLiteral("二维码内容：%1").arg(output.barcode.text));
        }
        lines.append(QStringLiteral("日期：%1").arg(output.dateState));
        if ((!output.barcodeWordResult.barcodeIsReadable
             || !output.barcodeWordResult.dateDetectionExecuted)
                && !output.reason.trimmed().isEmpty()) {
            lines.append(QStringLiteral("原因：%1").arg(output.reason));
        }
        result.presentationText = lines.join(QStringLiteral("\n"));
        result.hasPresentationText = true;
        applyDescriptorPolicy(descriptor, &result);
        return completeWith(frame, result);
    };
    return creation;
}
