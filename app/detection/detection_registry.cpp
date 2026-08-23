// 文件作用：本文件在Detection内完成预处理、定位和模式算法装配。
#include "detection/detection_registry.h"

#include "detection/detectionmode/barcode_word/barcode_word_detection_pipeline.h"
#include "detection/common/character_template_matcher.h"
#include "detection/detectionmode/ocr/ocr_detection_pipeline.h"
#include "detection/common/inspection_positioner.h"
#include "detection/detectionmode/stamp/overlap_detector.h"
#include "detection/detectionmode/stamp/stamp_detection_pipeline.h"
#include "detection/detectionmode/tissue/tissue_detection_pipeline.h"
#include "detection/detectionmode/word/word_detection_pipeline.h"
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
    const PreparedTemplate &prepared,
    StampConfiguration *configuration,
    QString *errorMessage)
{
    if (!configuration) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("钢印运行模板不可用。");
        }
        return false;
    }
    configuration->targetText = prepared.settings.targetText;
    configuration->thresholdPercent = prepared.settings.imageThresholdPercent;
    configuration->preparedTemplates = CharacterGlyphMatcher::prepare(
                prepared.characterTemplates);
    configuration->templateTargetIndexes =
            prepared.characterTemplateTargetIndexes;
    if (!configuration->preparedTemplates.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("钢印字符模板或图像阈值无效。");
        }
        return false;
    }

    const std::shared_ptr<OverlapDetector> overlap(new OverlapDetector);
    StampRegionData regions;
    regions.stamp_poly = prepared.stampPolygon;
    regions.date_poly = prepared.datePolygon;
    regions.barcode_poly = prepared.barcodePolygon;
    if (!overlap->init(prepared.stampRingTemplate, regions)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "钢印模板无法初始化防重叠检测资源。");
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

void setTemplatePresentation(
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
    const DetectionModeDescriptor descriptor = detectionModeDescriptor(request.mode);
    if (request.mode != DetectionMode::Tissue
            && request.preparedTemplates.isEmpty()) {
        creation.errorMessage = QStringLiteral("当前模式没有可用运行模板。");
        return creation;
    }
    for (const PreparedTemplateSnapshot &prepared :
         request.preparedTemplates) {
        if (!prepared || prepared->settings.detectionMode != request.mode) {
            creation.errorMessage = QStringLiteral("运行模板模式不匹配。");
            return creation;
        }
    }
    const PreparedTemplate *first = request.preparedTemplates.isEmpty()
            ? nullptr : request.preparedTemplates.first().get();

    StampConfiguration stampConfiguration;
    if (request.mode == DetectionMode::Stamp
            && !buildStampConfiguration(
                *first, &stampConfiguration, &creation.errorMessage)) {
        return creation;
    }
    if (request.mode == DetectionMode::Ocr
            && (!m_ocrEngine || !first)) {
        creation.errorMessage = QStringLiteral(
                    "深度OCR引擎或目标文本未初始化。");
        return creation;
    }
    if (descriptor.trackingKind == DetectionTrackingKind::MultipleTemplates
            && !request.templateSnapshot.isValid()) {
        creation.errorMessage = request.mode == DetectionMode::BarcodeWord
                ? QStringLiteral("二维码+三期运行模板快照未准备。")
                : QStringLiteral("字库运行模板快照未准备。");
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
                request.templateSnapshot.trackingTemplates,
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
                    new TissueDetectionPipeline(
                        request.tissueRoughnessThreshold));
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
        const std::string target = first->settings.targetText.toStdString();
        const QString templateName = first->displayName;
        creation.executor = [pipeline, positioner, preprocess, descriptor,
                engine, target, templateName](
            const std::shared_ptr<const FrameData> &source) {
            const std::shared_ptr<const FrameData> frame = preprocessFrame(source, preprocess);
            if (!frame) return DetectionCompletion();
            const DetectionPose pose = positioner->locate(frame->originalImage);
            DetectionResult result = pipeline->detect(
                        makeDetectionWorkItem(frame, pose), target, *engine);
            result.presentationText = result.recognizedText;
            result.hasPresentationText = true;
            setTemplatePresentation(templateName, pose, &result);
            applyDescriptorPolicy(descriptor, &result);
            return completeWith(frame, result);
        };
        return creation;
    }

    if (request.mode == DetectionMode::Stamp) {
        const std::shared_ptr<StampDetectionPipeline> pipeline(new StampDetectionPipeline);
        const QString templateName = first->displayName;
        creation.executor = [pipeline, positioner, preprocess, descriptor,
                stampConfiguration, templateName](
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
            setTemplatePresentation(templateName, output.pose, &result);
            applyDescriptorPolicy(descriptor, &result);
            return completeWith(frame, result);
        };
        return creation;
    }

    const std::shared_ptr<std::vector<DetectionModeWorkerTemplate> > templates(
                new std::vector<DetectionModeWorkerTemplate>(
                    request.templateSnapshot.detectionTemplates));
    if (request.mode == DetectionMode::Word) {
        const std::shared_ptr<WordDetectionPipeline> pipeline(new WordDetectionPipeline);
        creation.executor = [pipeline, positioner, preprocess, descriptor, templates](
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
            } else if (pose.wordTemplateIndex >= 0
                       && pose.wordTemplateIndex
                          < static_cast<int>(templates->size())) {
                const DetectionModeWorkerTemplate &selected = templates->at(
                            static_cast<std::size_t>(pose.wordTemplateIndex));
                output = pipeline->detect(item, selected.targetText,
                                          selected.templateName,
                                          selected.preparedTemplates,
                                          selected.templateTargetIndexes,
                                          selected.thresholdPercent);
            } else {
                output.pose = pose;
                output.detectionResult.modeId = detectionModeUiId(
                            DetectionMode::Word);
                output.detectionResult.status = DetectionStatus::Cancelled;
                output.detectionResult.diagnostic = QStringLiteral(
                            "Invalid word template index");
            }
            DetectionResult result = output.detectionResult;
            setTemplatePresentation(output.templateName, output.pose, &result);
            applyDescriptorPolicy(descriptor, &result);
            return completeWith(frame, result);
        };
        return creation;
    }

    const std::shared_ptr<BarcodeWordDetectionPipeline> pipeline(
                new BarcodeWordDetectionPipeline);
    IBarcodeDecoder *decoder = m_barcodeDecoder.get();
    creation.executor = [pipeline, positioner, preprocess, descriptor, templates, decoder](
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
        } else if (pose.wordTemplateIndex >= 0
                   && pose.wordTemplateIndex
                      < static_cast<int>(templates->size())) {
            DetectionModeWorkerTemplate &selected = templates->at(
                        static_cast<std::size_t>(pose.wordTemplateIndex));
            output = pipeline->detect(item, selected.targetText,
                                      selected.templateName,
                                      selected.preparedTemplates,
                                      selected.templateTargetIndexes,
                                      selected.thresholdPercent,
                                      selected.barcodeOptions,
                                      selected.decodeStrategy, decoder);
            selected.decodeStrategy = output.nextDecodeStrategy;
        } else {
            output.pose = pose;
            output.detectionResult.modeId = detectionModeUiId(
                        DetectionMode::BarcodeWord);
            output.detectionResult.status = DetectionStatus::Cancelled;
            output.detectionResult.diagnostic = QStringLiteral(
                        "Invalid barcode-word template index");
        }
        DetectionResult result = output.detectionResult;
        setTemplatePresentation(output.templateName, output.pose, &result);
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
