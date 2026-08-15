#include "ui/controllers/detection_completion_controller.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>

namespace {
QString normalizedFormat(QString format)
{
    format = format.trimmed();
    if (format.startsWith(QStringLiteral("."))) {
        format.remove(0, 1);
    }
    return format.isEmpty()
            ? QStringLiteral("png")
            : format.toLower();
}

ImageSaveItem saveItem(
    const QImage &image,
    const std::shared_ptr<const FrameData> &frame,
    const QString &directoryPath,
    const QString &baseName,
    const QString &format)
{
    ImageSaveItem item;
    item.image = image;
    item.frame = frame;
    item.filePath = QDir(directoryPath).filePath(
                baseName + QStringLiteral(".") + format);
    item.format = format.toUpper().toLatin1();
    return item;
}

QString resultDirectoryName(DetectionResultSaveAction action)
{
    return action == DetectionResultSaveAction::SaveNg
            ? QStringLiteral("ng")
            : QStringLiteral("ok");
}
}

DetectionCompletionController::DetectionCompletionController(
    InspectionRuntimeController *runtimeController,
    ImageSaveService *imageSaveService,
    DetectionResultPresenter *resultPresenter,
    const DetectionCompletionControllerCallbacks &callbacks)
    : m_runtimeController(runtimeController),
      m_imageSaveService(imageSaveService),
      m_resultPresenter(resultPresenter),
      m_callbacks(callbacks)
{
}

DetectionCompletionProcessOutcome
DetectionCompletionController::process(
    const DetectionCompletionProcessRequest &request)
{
    DetectionCompletionProcessOutcome result;
    if (!m_runtimeController
            || !m_resultPresenter
            || !request.completion.isValid()) {
        return result;
    }

    if (m_runtimeController->consumeDueDelayedNgRequest()) {
        result.delayedNgRequested = true;
        qDebug() << "[RESULT_HANDLER] Triggering delayed NG PLC request,"
                 << "totalCount:"
                 << m_runtimeController->totalCount();
        requestPlc(DetectionPlcAction::RequestNg);
    }

    DetectionResultViewSnapshot snapshot;
    if (request.preparePresentation) {
        snapshot = request.preparePresentation();
    }

    const DetectionResultHandlingOutcome handlingOutcome =
            m_runtimeController->record(
                request.completion,
                request.imageSaveModeIndex,
                request.delayedNgOffset);
    result.resultRecorded = handlingOutcome.resultRecorded;
    result.plcAction = handlingOutcome.plcAction;
    result.statistics = handlingOutcome.statistics;
    if (!handlingOutcome.resultRecorded) {
        return result;
    }

    result.imageSaveRequested =
            handlingOutcome.imageSaveAction
            != DetectionResultSaveAction::DoNotSave
            && (request.completion.result.verdict
                != AlgorithmVerdict::NotEvaluated
                || request.saveOptions.saveNotEvaluatedAsNg);
    if (result.imageSaveRequested) {
        result.imageSaveSubmitted = submitImageSave(
                    request,
                    handlingOutcome,
                    snapshot.image);
    }

    if (request.finalizePresentation) {
        request.finalizePresentation(&snapshot);
    }
    snapshot.productKey = request.completion.frame->productKey;
    snapshot.statistics = handlingOutcome.statistics;
    result.presentationAccepted = m_resultPresenter->present(snapshot);
    if (!result.presentationAccepted) {
        qWarning() << "[RESULT_PRESENTER] rejected snapshot"
                   << snapshot.productKey.runId
                   << snapshot.productKey.sequence;
    }

    requestPlc(handlingOutcome.plcAction);
    return result;
}

bool DetectionCompletionController::submitImageSave(
    const DetectionCompletionProcessRequest &request,
    const DetectionResultHandlingOutcome &handlingOutcome,
    const QImage &annotatedImage) const
{
    const DetectionCompletionSaveOptions &options =
            request.saveOptions;
    if (!m_imageSaveService) {
        qDebug() << (options.layout
                     == DetectionCompletionSaveLayout::RawOnly
                     ? QStringLiteral(
                         "\u4fdd\u5b58\u5931\u8d25\uff0c"
                         "\u5b58\u56fe\u670d\u52a1\u4e0d\u53ef\u7528")
                     : QStringLiteral(
                         "\u68c0\u6d4b\u56fe\u50cf\u4fdd\u5b58\u5931\u8d25\uff0c"
                         "\u5b58\u56fe\u670d\u52a1\u4e0d\u53ef\u7528"));
        return false;
    }

    if (options.layout
            == DetectionCompletionSaveLayout::AnnotatedAndRaw
            && options.rootDirectory.trimmed().isEmpty()) {
        qDebug() << QStringLiteral(
                    "\u68c0\u6d4b\u56fe\u50cf\u4fdd\u5b58\u5931\u8d25\uff0c"
                    "\u56fe\u50cf\u4fdd\u5b58\u8def\u5f84\u4e3a\u7a7a");
        return false;
    }

    const QString format = normalizedFormat(options.format);
    const QString resultName = resultDirectoryName(
                handlingOutcome.imageSaveAction);
    ImageSaveTask task;
    task.productKey = request.completion.frame->productKey;

    if (options.layout == DetectionCompletionSaveLayout::RawOnly) {
        const QString directoryPath =
                options.rootDirectory
                + QStringLiteral("/")
                + resultName
                + QStringLiteral("/");
        task.items.push_back(saveItem(
                                 QImage(),
                                 request.completion.frame,
                                 directoryPath,
                                 QDateTime::currentDateTime().toString(
                                     QStringLiteral("yyyyMMdd-hhmmss-zzz")),
                                 format));
    } else {
        const QString baseName =
                QDateTime::currentDateTime().toString(
                    QStringLiteral("yyyyMMdd-hhmmss.zzz"));
        const bool saveAnnotated =
                options.imageContentModeIndex == 0
                || options.imageContentModeIndex == 1;
        const bool saveRaw =
                options.imageContentModeIndex == 0
                || options.imageContentModeIndex == 2;

        if (saveAnnotated) {
            if (annotatedImage.isNull()) {
                if (m_callbacks.warnMissingAnnotatedImage) {
                    m_callbacks.warnMissingAnnotatedImage();
                }
            } else {
                task.items.push_back(saveItem(
                                         annotatedImage,
                                         std::shared_ptr<const FrameData>(),
                                         options.rootDirectory
                                         + QStringLiteral("/")
                                         + resultName
                                         + QStringLiteral("/"),
                                         baseName,
                                         format));
            }
        }
        if (saveRaw) {
            task.items.push_back(saveItem(
                                     QImage(),
                                     request.completion.frame,
                                     options.rootDirectory
                                     + QStringLiteral("/")
                                     + resultName
                                     + QStringLiteral("_raw/"),
                                     baseName,
                                     format));
        }
    }

    if (task.items.empty()) {
        qDebug() << QStringLiteral(
                    "\u68c0\u6d4b\u56fe\u50cf\u4fdd\u5b58\u4efb\u52a1\u4e3a\u7a7a");
        return false;
    }

    const ImageSaveSubmitResult submitResult =
            m_imageSaveService->submit(task);
    if (!submitResult.isAccepted()) {
        qDebug() << (options.layout
                     == DetectionCompletionSaveLayout::RawOnly
                     ? QStringLiteral(
                         "\u4fdd\u5b58\u4efb\u52a1\u63d0\u4ea4\u5931\u8d25\uff0c"
                         "\u72b6\u6001\uff1a")
                     : QStringLiteral(
                         "\u68c0\u6d4b\u56fe\u50cf\u4fdd\u5b58\u4efb\u52a1"
                         "\u63d0\u4ea4\u5931\u8d25\uff0c\u72b6\u6001\uff1a"))
                 << static_cast<int>(submitResult.status);
        return false;
    }
    return true;
}

void DetectionCompletionController::requestPlc(
    DetectionPlcAction action) const
{
    if (action == DetectionPlcAction::NoRequest
            || !m_callbacks.requestPlc) {
        return;
    }
    m_callbacks.requestPlc(action);
}
