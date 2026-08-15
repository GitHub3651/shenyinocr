#include "ui/controllers/inspection_result_coordinator.h"

#include "runtime/inspection_runtime_start_transaction.h"

#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QStringList>

namespace {

DetectionVerdictViewStyle verdictStyle(AlgorithmVerdict verdict)
{
    return verdict == AlgorithmVerdict::Ok
            ? DetectionVerdictViewStyle::Correct
            : DetectionVerdictViewStyle::Error;
}

QString profileTemplateName(
    const QString &templateName,
    const DetectionPose &pose,
    bool *updatesTemplateName)
{
    if (updatesTemplateName) {
        *updatesTemplateName = false;
    }
    if (!templateName.trimmed().isEmpty()) {
        if (updatesTemplateName) {
            *updatesTemplateName = true;
        }
        return templateName;
    }
    if (!pose.valid) {
        if (updatesTemplateName) {
            *updatesTemplateName = true;
        }
        return QStringLiteral("--");
    }
    return QString();
}

} // namespace

InspectionResultCoordinator::InspectionResultCoordinator(
    InspectionRuntimeController *runtimeController,
    const InspectionResultCoordinatorCallbacks &callbacks,
    QObject *parent)
    : QObject(parent),
      m_runtimeController(runtimeController),
      m_callbacks(callbacks),
      m_imageSaveService(new ImageSaveService(
          32,
          ImageSaveService::WriteFunction(),
          2))
{
    connect(m_imageSaveService.get(),
            &ImageSaveService::taskFailed,
            this,
            [this](quint64 totalFailed, const QString &latestError) {
        if (m_callbacks.reportImageSaveFailure) {
            m_callbacks.reportImageSaveFailure(
                        totalFailed,
                        latestError);
        }
    },
    Qt::QueuedConnection);

    DetectionCompletionControllerCallbacks completionCallbacks;
    completionCallbacks.requestPlc = [this](
            DetectionPlcAction action,
            const ProductKey &productKey) {
        if (m_runConfiguration.plcOutputEnabled
                && m_callbacks.requestPlc) {
            m_callbacks.requestPlc(action, productKey);
        }
    };
    completionCallbacks.warnMissingAnnotatedImage =
            m_callbacks.warnMissingAnnotatedImage;
    m_completionController.reset(
                new DetectionCompletionController(
                    m_runtimeController,
                    m_imageSaveService.get(),
                    &m_presenter,
                    completionCallbacks));
}

InspectionResultCoordinator::~InspectionResultCoordinator()
{
    shutdown();
}

void InspectionResultCoordinator::bindView(
    const DetectionResultViewBindings &bindings)
{
    m_presenter.bindView(bindings);
}

void InspectionResultCoordinator::configureRun(
    const InspectionResultRunConfiguration &configuration)
{
    m_runConfiguration = configuration;
}

bool InspectionResultCoordinator::requiresPlcForRun() const
{
    return m_runConfiguration.plcOutputEnabled;
}

DetectionModeWorkerConsumers
InspectionResultCoordinator::workerConsumers()
{
    DetectionModeWorkerConsumers consumers;
    consumers.tissue = [this](
            const DetectionCompletion &completion,
            const TissueRollResult &tissueResult) {
        postUiWork([this, completion, tissueResult]() {
            handleTissueCompletion(completion, tissueResult);
        });
    };
    consumers.ocr = [this](
            const DetectionCompletion &completion,
            const DetectionPose &pose) {
        postUiWork([this, completion, pose]() {
            handleOcrCompletion(completion, pose);
        });
    };
    consumers.stamp = [this](
            const DetectionCompletion &completion,
            const StampDetectionWorkOutput &output) {
        postUiWork([this, completion, output]() {
            handleStampCompletion(completion, output);
        });
    };
    consumers.word = [this](
            const DetectionCompletion &completion,
            const WordDetectionWorkOutput &output) {
        postUiWork([this, completion, output]() {
            handleWordCompletion(completion, output);
        });
    };
    consumers.barcodeWord = [this](
            const DetectionCompletion &completion,
            const BarcodeWordDetectionWorkOutput &output) {
        postUiWork([this, completion, output]() {
            handleBarcodeWordCompletion(completion, output);
        });
    };
    return consumers;
}

bool InspectionResultCoordinator::startDetectionWorker(
    InspectionRuntimeStartTransaction &startTransaction,
    const InspectionDetectionWorkerStartConfiguration &configuration,
    QString *errorMessage)
{
    DetectionModeWorkerRequest request;
    request.modeIndex = configuration.modeIndex;
    request.profiles = configuration.profiles;

    switch (configuration.modeIndex) {
    case 0:
        if (!configuration.stampThresholdValid
                || configuration.stampTemplates.empty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "\u94a2\u5370\u5b57\u7b26\u6a21\u677f\u6216"
                            "\u56fe\u50cf\u9608\u503c\u65e0\u6548\u3002");
            }
            return false;
        }
        request.stampConfiguration.thresholdPercent =
                configuration.stampThresholdPercent;
        request.stampConfiguration.targetText =
                configuration.targetText;
        request.stampConfiguration.preparedTemplates =
                CharacterTemplateMatcher::prepare(
                    configuration.stampTemplates);
        request.stampConfiguration.templateTargetIndexes.reserve(
                    configuration.stampTemplates.size());
        for (int index = 0;
             index < static_cast<int>(
                 configuration.stampTemplates.size());
             ++index) {
            request.stampConfiguration.templateTargetIndexes.push_back(
                        index);
        }
        request.stampConfiguration.detectOverlap =
                configuration.detectStampOverlap;
        break;
    case 1:
        break;
    case 2:
        request.ocrEngine = configuration.ocrEngine;
        if (request.ocrEngine) {
            request.targetText =
                    configuration.targetText.toStdString();
        }
        break;
    case 3:
        request.tissueParameters = configuration.tissueParameters;
        break;
    case 4:
        request.barcodeDecoder = configuration.barcodeDecoder;
        break;
    default:
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "\u4e0d\u652f\u6301\u7684\u68c0\u6d4b\u6a21\u5f0f\u3002");
        }
        return false;
    }

    configureRun(configuration.resultConfiguration);
    const DetectionWorker::FailureConsumer failureConsumer =
            [this](const QString &message) {
        QMetaObject::invokeMethod(
                    this,
                    [message]() {
            qWarning() << "[DETECTION_WORKER]" << message;
        },
        Qt::QueuedConnection);
    };
    const DetectionModeWorkerCreationResult creation =
            DetectionModeWorkerDispatcher::create(
                request,
                workerConsumers(),
                failureConsumer);
    if (!creation.isAccepted()) {
        if (errorMessage) {
            *errorMessage = creation.errorMessage;
        }
        return false;
    }
    if (!startTransaction.startDetectionWorker(
                configuration.modeIndex,
                creation.worker)) {
        if (errorMessage) {
            *errorMessage = creation.startFailureMessage;
        }
        return false;
    }

    qDebug() << "[DETECTION_WORKER]"
             << qPrintable(creation.workerLogName)
             << "worker started"
             << "queueCapacity="
             << static_cast<qulonglong>(
                 m_runtimeController
                 ? m_runtimeController->detectionWorkerQueueCapacity()
                 : 0);
    return true;
}

void InspectionResultCoordinator::clear()
{
    m_presenter.clear();
}

void InspectionResultCoordinator::clearTransientView()
{
    m_presenter.clearTransientView();
}

bool InspectionResultCoordinator::renderAndPresentFrame(
    const cv::Mat &image,
    bool includeTissueOverlay)
{
    const QImage rendered = m_presenter.renderFrame(
                image,
                includeTissueOverlay);
    return !rendered.isNull() && m_presenter.presentFrame(rendered);
}

bool InspectionResultCoordinator::presentPreviewFrame(
    const cv::Mat &image,
    bool tissueMode,
    bool productionRunning)
{
    if (image.empty() || (tissueMode && productionRunning)) {
        return false;
    }
    return renderAndPresentFrame(image, tissueMode);
}

void InspectionResultCoordinator::updatePose(
    const DetectionPose &pose)
{
    m_presenter.updatePose(pose);
}

void InspectionResultCoordinator::presentTotalAndNgCounts(
    int totalCount,
    int ngCount)
{
    m_presenter.presentTotalAndNgCounts(totalCount, ngCount);
}

void InspectionResultCoordinator::presentNgCount(int ngCount)
{
    m_presenter.presentNgCount(ngCount);
}

void InspectionResultCoordinator::shutdown()
{
    m_completionController.reset();
    if (m_imageSaveService) {
        m_imageSaveService->shutdown();
        m_imageSaveService.reset();
    }
}

bool InspectionResultCoordinator::postUiWork(
    const UiCompletionMailbox::Work &work)
{
    if (!m_runtimeController
            || !m_runtimeController->submitUiCompletion(work)) {
        return false;
    }

    const bool posted = QMetaObject::invokeMethod(
                this,
                [this]() {
        if (!m_runtimeController->processOneUiCompletion()) {
            qDebug() << "[UI_COMPLETION] cancelled or empty UI work ignored";
        }
    },
    Qt::QueuedConnection);
    if (!posted) {
        m_runtimeController->cancelUiCompletion();
        qWarning() << "[UI_COMPLETION] unable to post result-bound UI work";
        return false;
    }
    return true;
}

DetectionCompletion InspectionResultCoordinator::acceptCompletion(
    const DetectionCompletion &completion,
    const char *modeName)
{
    if (!m_runtimeController) {
        return DetectionCompletion();
    }
    const DetectionCompletion accepted =
            m_runtimeController->complete(
                completion.frame,
                completion.result);
    if (!accepted.isValid()) {
        qDebug() << "[DETECTION_WORKER] stale"
                 << modeName
                 << "completion ignored";
    }
    return accepted;
}

DetectionCompletionProcessRequest
InspectionResultCoordinator::baseRequest(
    const DetectionCompletion &completion) const
{
    DetectionCompletionProcessRequest request;
    request.completion = completion;
    request.imageSaveModeIndex =
            m_runConfiguration.imageSaveModeIndex;
    request.delayedNgOffset =
            m_runConfiguration.delayedNgOffset;
    request.saveOptions = m_runConfiguration.saveOptions;
    return request;
}

qint64 InspectionResultCoordinator::presentationElapsedMs(
    const DetectionCompletion &completion)
{
    qint64 elapsedMs = static_cast<qint64>(
                completion.result.elapsedMs + 0.5);
    if (!completion.frame
            || !completion.frame->timestampUtc.isValid()) {
        return elapsedMs;
    }

    const qint64 endToEndMs =
            completion.frame->timestampUtc.msecsTo(
                QDateTime::currentDateTimeUtc());
    return endToEndMs > elapsedMs
            ? endToEndMs
            : elapsedMs;
}

void InspectionResultCoordinator::handleTissueCompletion(
    const DetectionCompletion &completion,
    const TissueRollResult &tissueResult)
{
    const DetectionCompletion accepted =
            acceptCompletion(completion, "tissue");
    if (accepted.isValid()) {
        finalizeTissue(tissueResult, accepted);
    }
}

void InspectionResultCoordinator::handleOcrCompletion(
    const DetectionCompletion &completion,
    const DetectionPose &pose)
{
    const DetectionCompletion accepted =
            acceptCompletion(completion, "OCR");
    if (!accepted.isValid()) {
        return;
    }
    if (accepted.result.status == DetectionStatus::Cancelled) {
        qDebug() << "[OCR_ERROR] Invalid selection area!";
        return;
    }
    finalizeOcr(pose, accepted);
}

void InspectionResultCoordinator::handleStampCompletion(
    const DetectionCompletion &completion,
    const StampDetectionWorkOutput &output)
{
    const DetectionCompletion accepted =
            acceptCompletion(completion, "stamp");
    if (!accepted.isValid()) {
        return;
    }
    if (accepted.result.status == DetectionStatus::Cancelled) {
        if (m_callbacks.showDetectionRoiWarning) {
            m_callbacks.showDetectionRoiWarning();
        }
        return;
    }
    if (m_callbacks.clearDetectionRoiWarning) {
        m_callbacks.clearDetectionRoiWarning();
    }
    finalizeStamp(output, accepted);
}

void InspectionResultCoordinator::handleWordCompletion(
    const DetectionCompletion &completion,
    const WordDetectionWorkOutput &output)
{
    const DetectionCompletion accepted =
            acceptCompletion(completion, "word");
    if (!accepted.isValid()) {
        return;
    }
    if (accepted.result.status == DetectionStatus::Cancelled) {
        qDebug() << "[WORD_DETECT] positioned work cancelled:"
                 << accepted.result.diagnostic;
        return;
    }
    finalizeWord(output, accepted);
}

void InspectionResultCoordinator::handleBarcodeWordCompletion(
    const DetectionCompletion &completion,
    const BarcodeWordDetectionWorkOutput &output)
{
    const DetectionCompletion accepted =
            acceptCompletion(completion, "barcode-word");
    if (!accepted.isValid()) {
        return;
    }
    if (accepted.result.status == DetectionStatus::Cancelled) {
        qDebug() << "[BARCODE_WORD] positioned work cancelled:"
                 << accepted.result.diagnostic;
        return;
    }
    finalizeBarcodeWord(output, accepted);
}

void InspectionResultCoordinator::finalizeTissue(
    const TissueRollResult &tissueResult,
    const DetectionCompletion &completion)
{
    if (!m_completionController) {
        qDebug() << "[TISSUE_DETECT] Invalid input image.";
        return;
    }

    const QString recognitionText = tissueResult.rollFound
            ? QStringLiteral("\u7c97\u7cd9\u5ea6\uff1a%1")
              .arg(tissueResult.roll.roughnessScore, 0, 'f', 3)
            : QStringLiteral("\u7c97\u7cd9\u5ea6\uff1a--");
    DetectionCompletionProcessRequest request = baseRequest(completion);
    request.preparePresentation =
            [this, completion, tissueResult, recognitionText]() {
        clearLegacyPresentationState(false);
        TissueRollPresentation tissuePresentation;
        if (tissueResult.rollFound) {
            tissuePresentation.center = tissueResult.roll.center;
            tissuePresentation.outerAxes = tissueResult.roll.outerAxes;
            tissuePresentation.innerCenter = tissueResult.roll.innerCenter;
            tissuePresentation.innerAxes = tissueResult.roll.innerAxes;
        }
        m_presenter.installTissueRoll(
                    tissuePresentation,
                    tissueResult.rollFound);

        qDebug() << "[TISSUE_DETECT]"
                 << QString::fromStdString(tissueResult.message);
        qDebug() << "[TISSUE_DETECT_DEBUG]"
                 << "image" << tissueResult.imageWidth
                 << "x" << tissueResult.imageHeight
                 << "processingTimeMs" << tissueResult.processingTimeMs
                 << "rollFound" << tissueResult.rollFound
                 << "overall" << (tissueResult.isOk ? "OK" : "NG");
        if (tissueResult.rollFound) {
            const TissueRollItem &roll = tissueResult.roll;
            qDebug() << "[TISSUE_DETECT_DEBUG]"
                     << (roll.isOk ? "OK" : "NG")
                     << "reason" << QString::fromStdString(roll.rejectReason)
                     << "rough" << roll.roughnessScore
                     << "roughNg" << roll.roughnessNg
                     << "ringPixels" << roll.ringPixelCount
                     << "outerCenter" << roll.center.x << roll.center.y
                     << "outerRadius" << roll.outerAxes.width
                     << "outerBbox" << roll.outerBbox.x << roll.outerBbox.y
                     << roll.outerBbox.width << roll.outerBbox.height
                     << "innerFound" << roll.innerHoleFound
                     << "innerCenter" << roll.innerCenter.x << roll.innerCenter.y
                     << "innerRadius" << roll.innerAxes.width;
        }

        DetectionResultViewSnapshot snapshot;
        snapshot.image = m_presenter.renderFrame(
                    completion.frame->originalImage,
                    true);
        snapshot.verdictStyle = verdictStyle(completion.result.verdict);
        snapshot.recognitionText = recognitionText;
        return snapshot;
    };
    request.finalizePresentation =
            [completion](DetectionResultViewSnapshot *snapshot) {
        snapshot->elapsedText = QStringLiteral(
                    "\u68c0\u6d4b\u8017\u65f6 %1 \u6beb\u79d2")
                .arg(presentationElapsedMs(completion));
    };

    const DetectionCompletionProcessOutcome outcome =
            m_completionController->process(request);
    if (!outcome.resultRecorded) {
        qWarning() << "[RUNTIME_CONTROLLER] rejected tissue completion";
    }
}

void InspectionResultCoordinator::finalizeOcr(
    const DetectionPose &pose,
    const DetectionCompletion &completion)
{
    if (!m_completionController) {
        return;
    }
    DetectionCompletionProcessRequest request = baseRequest(completion);
    request.saveOptions.layout = DetectionCompletionSaveLayout::RawOnly;
    request.preparePresentation = [this, completion, pose]() {
        clearLegacyPresentationState(false);
        qDebug() << "----------------- OCR PROCESS START -----------------";
        m_presenter.installDetectionResult(completion.result, pose);
        if (m_callbacks.storeLegacyRecognitionText) {
            m_callbacks.storeLegacyRecognitionText(
                        completion.result.recognizedText);
        }
        qDebug() << "[OCR_LOG] Final String:"
                 << completion.result.recognizedText;

        DetectionResultViewSnapshot snapshot;
        snapshot.image = m_presenter.renderFrame(
                    completion.frame->originalImage,
                    false);
        snapshot.verdictStyle = verdictStyle(completion.result.verdict);
        snapshot.recognitionText = completion.result.recognizedText;
        return snapshot;
    };
    request.finalizePresentation =
            [completion](DetectionResultViewSnapshot *snapshot) {
        snapshot->elapsedText = QStringLiteral(
                    "\u68c0\u6d4b\u8017\u65f6 %1 \u6beb\u79d2")
                .arg(presentationElapsedMs(completion));
    };
    const DetectionCompletionProcessOutcome outcome =
            m_completionController->process(request);
    if (!outcome.resultRecorded) {
        qWarning() << "[RUNTIME_CONTROLLER] rejected OCR completion";
        return;
    }
    qDebug() << "----------------- OCR PROCESS END -----------------";
}

void InspectionResultCoordinator::finalizeStamp(
    const StampDetectionWorkOutput &output,
    const DetectionCompletion &completion)
{
    if (!m_completionController) {
        return;
    }
    DetectionCompletionProcessRequest request = baseRequest(completion);
    request.saveOptions.saveNotEvaluatedAsNg = false;
    request.preparePresentation = [this, completion, output]() {
        clearLegacyPresentationState(true);
        m_presenter.installDetectionResult(
                    completion.result,
                    output.pose,
                    output.hasOverlapDetection
                    && !output.stampResult.overlapIsOk);
        DetectionResultViewSnapshot snapshot;
        snapshot.image = m_presenter.renderFrame(
                    completion.frame->originalImage,
                    false);
        snapshot.verdictStyle = verdictStyle(completion.result.verdict);
        return snapshot;
    };
    request.finalizePresentation =
            [completion](DetectionResultViewSnapshot *snapshot) {
        snapshot->elapsedText = QStringLiteral(
                    "\u68c0\u6d4b\u8017\u65f6 %1 \u6beb\u79d2")
                .arg(presentationElapsedMs(completion));
    };
    const DetectionCompletionProcessOutcome outcome =
            m_completionController->process(request);
    if (!outcome.resultRecorded) {
        qWarning() << "[RUNTIME_CONTROLLER] rejected stamp completion";
    }
}

void InspectionResultCoordinator::finalizeWord(
    const WordDetectionWorkOutput &output,
    const DetectionCompletion &completion)
{
    if (!m_completionController) {
        return;
    }
    bool updatesTemplateName = false;
    const QString templateName = profileTemplateName(
                output.templateName,
                output.pose,
                &updatesTemplateName);
    DetectionCompletionProcessRequest request = baseRequest(completion);
    request.saveOptions.saveNotEvaluatedAsNg = false;
    request.preparePresentation =
            [this, completion, output,
             updatesTemplateName, templateName]() {
        clearLegacyPresentationState(true);
        m_presenter.installDetectionResult(completion.result, output.pose);
        DetectionResultViewSnapshot snapshot;
        snapshot.image = m_presenter.renderFrame(
                    completion.frame->originalImage,
                    false);
        snapshot.verdictStyle = verdictStyle(completion.result.verdict);
        snapshot.updatesTemplateName = updatesTemplateName;
        snapshot.templateName = templateName;
        return snapshot;
    };
    request.finalizePresentation =
            [completion](DetectionResultViewSnapshot *snapshot) {
        snapshot->elapsedText = QStringLiteral(
                    "\u68c0\u6d4b\u8017\u65f6 %1 \u6beb\u79d2")
                .arg(presentationElapsedMs(completion));
    };
    const DetectionCompletionProcessOutcome outcome =
            m_completionController->process(request);
    if (!outcome.resultRecorded) {
        qWarning() << "[RUNTIME_CONTROLLER] rejected word completion";
        return;
    }
    qDebug().noquote()
            << QString("[WORD_DETECT] template=%1 result=%2 reason=%3 "
                       "targetCount=%4 detectedCount=%5 poseScore=%6")
               .arg(output.templateName.isEmpty()
                    ? QStringLiteral("--")
                    : output.templateName)
               .arg(completion.result.verdict == AlgorithmVerdict::Ok
                    ? QStringLiteral("OK")
                    : QStringLiteral("NG"))
               .arg(completion.result.diagnostic)
               .arg(output.wordResult.targetCharacterCount)
               .arg(output.wordResult.detectedCharacterCount)
               .arg(output.pose.score, 0, 'f', 4);
}

void InspectionResultCoordinator::finalizeBarcodeWord(
    const BarcodeWordDetectionWorkOutput &output,
    const DetectionCompletion &completion)
{
    if (!m_completionController) {
        return;
    }
    bool updatesTemplateName = false;
    const QString templateName = profileTemplateName(
                output.templateName,
                output.pose,
                &updatesTemplateName);
    QStringList resultLines;
    resultLines.append(QStringLiteral("\u4e8c\u7ef4\u7801\uff1a%1")
                       .arg(output.barcodeState));
    if (!output.barcode.text.isEmpty()) {
        resultLines.append(
                    QStringLiteral("\u4e8c\u7ef4\u7801\u5185\u5bb9\uff1a%1")
                    .arg(output.barcode.text));
    }
    resultLines.append(QStringLiteral("\u65e5\u671f\uff1a%1")
                       .arg(output.dateState));
    if ((!output.barcodeWordResult.barcodeIsReadable
         || !output.barcodeWordResult.dateDetectionExecuted)
            && !output.reason.trimmed().isEmpty()) {
        resultLines.append(QStringLiteral("\u539f\u56e0\uff1a%1")
                           .arg(output.reason));
    }

    double elapsedMs = 0.0;
    DetectionCompletionProcessRequest request = baseRequest(completion);
    request.saveOptions.saveNotEvaluatedAsNg = false;
    request.preparePresentation =
            [this, completion, output, resultLines,
             updatesTemplateName, templateName]() {
        clearLegacyPresentationState(true);
        m_presenter.installDetectionResult(completion.result, output.pose);
        DetectionResultViewSnapshot snapshot;
        snapshot.image = m_presenter.renderFrame(
                    completion.frame->originalImage,
                    false);
        snapshot.verdictStyle = verdictStyle(completion.result.verdict);
        snapshot.recognitionText = resultLines.join(QStringLiteral("\n"));
        snapshot.updatesTemplateName = updatesTemplateName;
        snapshot.templateName = templateName;
        return snapshot;
    };
    request.finalizePresentation =
            [completion, &elapsedMs](
                DetectionResultViewSnapshot *snapshot) {
        elapsedMs = static_cast<double>(
                    presentationElapsedMs(completion));
        snapshot->elapsedText = QStringLiteral(
                    "\u68c0\u6d4b\u8017\u65f6 %1 ms")
                .arg(elapsedMs, 0, 'f', 2);
    };
    const DetectionCompletionProcessOutcome outcome =
            m_completionController->process(request);
    if (!outcome.resultRecorded) {
        qWarning() << "[RUNTIME_CONTROLLER] rejected barcode-word completion";
        return;
    }
    qDebug().noquote()
            << QString("[BARCODE_WORD] template=%1 barcode=%2 date=%3 "
                       "final=%4 trackingMs=%5 barcodeMs=%6 totalMs=%7 "
                       "reason=%8 decoderReason=%9")
               .arg(output.templateName.isEmpty()
                    ? QStringLiteral("--")
                    : output.templateName)
               .arg(output.barcodeState)
               .arg(output.dateState)
               .arg(completion.result.verdict == AlgorithmVerdict::Ok
                    ? QStringLiteral("OK")
                    : QStringLiteral("NG"))
               .arg(output.pose.trackingElapsedMs, 0, 'f', 3)
               .arg(output.barcode.elapsedMs, 0, 'f', 3)
               .arg(elapsedMs, 0, 'f', 3)
               .arg(output.reason)
               .arg(output.barcode.errorReason);
}

void InspectionResultCoordinator::clearLegacyPresentationState(
    bool clearImageLabelRects) const
{
    if (m_callbacks.clearLegacyPresentationState) {
        m_callbacks.clearLegacyPresentationState(clearImageLabelRects);
    }
}
