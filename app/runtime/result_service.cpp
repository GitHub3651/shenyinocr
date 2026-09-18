#include "runtime/result_service.h"

#include "contracts/detection_mode.h"
#include "runtime/inspection_presentation_renderer.h"
#include "runtime/inspection_runtime.h"
#include "system_support/logging/log_categories.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QThread>

#include <chrono>

namespace {

QString normalizedFormat(QString format)
{
    format = format.trimmed();
    if (format.startsWith(QStringLiteral("."))) {
        format.remove(0, 1);
    }
    return format.isEmpty() ? QStringLiteral("png") : format.toLower();
}

QString resultDirectoryName(DetectionResultSaveAction action)
{
    return action == DetectionResultSaveAction::SaveNg
            ? QStringLiteral("ng")
            : QStringLiteral("ok");
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

DetectionVerdictViewStyle verdictStyle(AlgorithmVerdict verdict)
{
    return verdict == AlgorithmVerdict::Ok
            ? DetectionVerdictViewStyle::Correct
            : DetectionVerdictViewStyle::Error;
}

QString verdictName(AlgorithmVerdict verdict)
{
    switch (verdict) {
    case AlgorithmVerdict::Ok:
        return QStringLiteral("OK");
    case AlgorithmVerdict::Ng:
        return QStringLiteral("NG");
    }
    return QStringLiteral("UNKNOWN");
}

QString stableModeId(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("-");
    }
    const DetectionModeDescriptor *descriptor =
            detectionModeDescriptorFromUiId(trimmed);
    if (!descriptor) {
        descriptor = detectionModeDescriptorFromId(trimmed);
    }
    return descriptor
            ? QString::fromLatin1(descriptor->modeId)
            : trimmed;
}

QString imageSaveSubmitStatusName(ImageSaveSubmitStatus status)
{
    switch (status) {
    case ImageSaveSubmitStatus::Accepted:
        return QStringLiteral("accepted");
    case ImageSaveSubmitStatus::InvalidTask:
        return QStringLiteral("invalid_task");
    case ImageSaveSubmitStatus::Stopping:
        return QStringLiteral("stopping");
    }
    return QStringLiteral("unknown");
}

QString finalScoreText(const DetectionResult &result)
{
    for (const DetectionOverlayPolygon &polygon : result.overlay.polygons) {
        if (polygon.role == QLatin1String("tracking")) {
            return QString::number(polygon.score, 'f', 3);
        }
    }
    return QString();
}

} // namespace

ResultService::ResultService(
    InspectionRuntime &runtime,
    QObject *parent)
    : QObject(parent),
      m_runtime(runtime),
      m_imageSaveService(new ImageSaveService(
          32,
          ImageSaveService::WriteFunction(),
          2))
{
    m_plcResetTimer.setSingleShot(true);
    m_plcResetTimer.setInterval(100);
    connect(&m_plcResetTimer,
            &QTimer::timeout,
            this,
            &ResultService::resetPlcPulse);
}

ResultService::~ResultService()
{
    shutdown();
}

void ResultService::configureRun(
    const ResultServiceRunConfiguration &configuration)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_runConfiguration = configuration;
    std::queue<DelayedNgRequest> empty;
    m_delayedNgRequests.swap(empty);
    m_pendingPlcResetProducts.clear();
}

bool ResultService::requiresPlcForRun() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_runConfiguration.plcOutputEnabled;
}

DetectionWorker::CompletionConsumer ResultService::completionConsumer()
{
    return [this](const DetectionCompletion &completion) {
        handleCompletion(completion);
    };
}

DetectionResultStatistics ResultService::statistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics;
}

void ResultService::resetStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statistics = DetectionResultStatistics();
}

void ResultService::resetNgCount()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statistics.ngCount = 0;
}

void ResultService::clearPendingDelayedNgRequests()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::queue<DelayedNgRequest> empty;
    m_delayedNgRequests.swap(empty);
}

void ResultService::shutdown()
{
    m_plcResetTimer.stop();
    if (m_imageSaveService) {
        m_imageSaveService->shutdown();
        m_imageSaveService.reset();
    }
}

DetectionCompletion ResultService::acceptCompletion(
    const DetectionCompletion &completion)
{
    const DetectionCompletion accepted = m_runtime.complete(
                completion.frame, completion.result);
    const InspectionRuntimeState state = m_runtime.state();
    if (!accepted.isValid()
            && (state == InspectionRuntimeState::Starting
                || state == InspectionRuntimeState::Running)) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=frame.completion_ignored mode=%1 reason=stale_or_invalid")
                   .arg(stableModeId(completion.result.modeId));
    }
    return accepted;
}

DetectionResultSaveAction ResultService::imageSaveActionFor(
    AlgorithmVerdict verdict) const
{
    int mode = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        mode = m_runConfiguration.imageSaveModeIndex;
    }
    if (verdict == AlgorithmVerdict::Ok) {
        return mode == 2 || mode == 3
                ? DetectionResultSaveAction::SaveOk
                : DetectionResultSaveAction::DoNotSave;
    }
    return mode == 1 || mode == 3
            ? DetectionResultSaveAction::SaveNg
            : DetectionResultSaveAction::DoNotSave;
}

bool ResultService::consumeDueDelayedNgRequest(ProductKey *productKey)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_delayedNgRequests.empty()
            || m_statistics.totalCount
               < m_delayedNgRequests.front().dueTotalCount - 1) {
        return false;
    }
    if (productKey) {
        *productKey = m_delayedNgRequests.front().productKey;
    }
    m_delayedNgRequests.pop();
    return true;
}

void ResultService::process(const ProcessRequest &request)
{
    if (!request.completion.isValid()) {
        return;
    }

    if (!m_runtime.claimResult(request.completion.frame->productKey)) {
        return;
    }

    const ProductKey productKey = request.completion.frame->productKey;
    bool barcodeCsvEnabled = false;
    QString barcodeCsvOutputDirectory;
    bool plcOutputEnabled = false;
    int delayedNgOffset = 0;
    DetectionResultStatistics candidateStatistics;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        barcodeCsvEnabled = m_runConfiguration.barcodeCsvEnabled;
        barcodeCsvOutputDirectory =
                m_runConfiguration.barcodeCsvOutputDirectory;
        plcOutputEnabled = m_runConfiguration.plcOutputEnabled;
        delayedNgOffset = m_runConfiguration.delayedNgOffset;
        candidateStatistics = m_statistics;
    }
    if (barcodeCsvEnabled
            && request.completion.result.verdict == AlgorithmVerdict::Ok) {
        QString filePath;
        QString errorMessage;
        if (!appendBarcodeCsvResult(
                    request.completion.result,
                    barcodeCsvOutputDirectory,
                    &filePath,
                    &errorMessage)) {
            qCCritical(logRuntime).noquote()
                    << QStringLiteral(
                        "event=barcode_csv.write_failed path=%1 error=%2")
                       .arg(filePath, errorMessage);
            m_runtime.enterFault(
                        InspectionFaultReason::BarcodeCsvUnavailable,
                        QStringLiteral("CSV 文件：%1；错误：%2")
                        .arg(filePath, errorMessage));
            return;
        }
    }

    ProductKey delayedProduct;
    if (plcOutputEnabled
            && consumeDueDelayedNgRequest(&delayedProduct)
            && !requestPlc(DetectionPlcAction::RequestNg, delayedProduct)) {
        return;
    }

    const DetectionResultSaveAction saveAction = imageSaveActionFor(
                request.completion.result.verdict);
    DetectionPlcAction plcAction = DetectionPlcAction::NoRequest;
    DelayedNgRequest delayedRequest;
    bool enqueueDelayedNg = false;
    ++candidateStatistics.totalCount;
    if (request.completion.result.verdict == AlgorithmVerdict::Ok) {
        if (plcOutputEnabled) {
            plcAction = DetectionPlcAction::RequestOk;
        }
    } else {
        ++candidateStatistics.ngCount;
        if (plcOutputEnabled) {
            if (delayedNgOffset == 0) {
                plcAction = DetectionPlcAction::RequestNg;
            } else {
                delayedRequest.dueTotalCount =
                        candidateStatistics.totalCount + delayedNgOffset;
                delayedRequest.productKey = productKey;
                enqueueDelayedNg = true;
            }
        }
    }

    InspectionPresentation presentation;
    presentation.image =
            InspectionPresentationRenderer::renderDetectionFrame(
                request.completion.frame->originalImage,
                request.completion.result.overlay);
    presentation.verdictStyle = verdictStyle(
                request.completion.result.verdict);
    presentation.recognitionText =
            request.completion.result.hasPresentationText
            ? request.completion.result.presentationText
            : QString();
    presentation.updatesTemplateName =
            request.completion.result.updatesTemplateName;
    presentation.templateName = request.completion.result.templateName;

    const bool imageSaveRequested =
            saveAction != DetectionResultSaveAction::DoNotSave;
    if (imageSaveRequested
            && !submitImageSave(request, saveAction, presentation.image)) {
        if (m_runtime.isRunning()) {
            m_runtime.enterFault(
                        InspectionFaultReason::RuntimeInvariantViolation,
                        QStringLiteral("图像保存任务提交失败。"));
        }
        return;
    }

    if (plcOutputEnabled
            && plcAction != DetectionPlcAction::NoRequest
            && !requestPlc(plcAction, productKey)) {
        return;
    }

    presentation.statistics = candidateStatistics;
    const double processingElapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now()
                - request.completion.frame->processingStartedAt).count();
    presentation.elapsedText = QStringLiteral("%1 ms")
            .arg(processingElapsedMs, 0, 'f', 2);
    if (!m_runtime.publishPresentation(presentation)) {
        if (m_runtime.isRunning()) {
            m_runtime.enterFault(
                        InspectionFaultReason::RuntimeInvariantViolation,
                        QStringLiteral("检测结果界面投递失败。"));
        }
        return;
    }
    if (m_runtime.state() == InspectionRuntimeState::Fault) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statistics = candidateStatistics;
        if (enqueueDelayedNg) {
            m_delayedNgRequests.push(delayedRequest);
        }
    }
    if (!m_runtime.finalizeResultClaim(productKey)) {
        m_runtime.enterFault(
                    InspectionFaultReason::RuntimeInvariantViolation,
                    QStringLiteral("产品正式结果收口失败。"));
        return;
    }

    const DetectionResult &result = request.completion.result;
    QString summary = QStringLiteral(
                "event=frame.completed mode=%1 result=%2 ms=%3")
            .arg(stableModeId(result.modeId))
            .arg(verdictName(result.verdict))
            .arg(processingElapsedMs, 0, 'f', 2);
    const QString templateName = result.templateName.trimmed();
    if (!templateName.isEmpty()
            && templateName != QLatin1String("--")) {
        summary += QStringLiteral(" template=%1")
                .arg(templateName);
    }
    const QString finalScore = finalScoreText(result);
    if (!finalScore.isEmpty()) {
        summary += QStringLiteral(" score=%1").arg(finalScore);
    }
    if (!result.recognizedText.trimmed().isEmpty()) {
        summary += QStringLiteral(" text=%1")
                .arg(result.recognizedText.trimmed());
    }
    if (result.verdict != AlgorithmVerdict::Ok
            && !result.diagnostic.trimmed().isEmpty()) {
        summary += QStringLiteral(" reason=%1")
                .arg(result.diagnostic.trimmed());
    }
    qCInfo(logDetection).noquote() << summary;
}

bool ResultService::appendBarcodeCsvResult(
    const DetectionResult &result,
    const QString &outputDirectory,
    QString *filePath,
    QString *errorMessage) const
{
    const QDateTime localNow = QDateTime::currentDateTime();
    const QString path = QDir(outputDirectory).filePath(
                QStringLiteral("qr_results_%1.csv")
                .arg(localNow.date().toString(QStringLiteral("yyyyMMdd"))));
    if (filePath) {
        *filePath = path;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QByteArray bytes;
    if (file.size() == 0) {
        bytes.append("\xEF\xBB\xBF", 3);
    }
    bytes.append(csvEscape(result.qrContent).toUtf8());
    bytes.append('\n');

    if (file.write(bytes) != bytes.size()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    if (!file.flush()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
}

QString ResultService::csvEscape(QString value)
{
    if (!value.contains(QLatin1Char(','))
            && !value.contains(QLatin1Char('"'))
            && !value.contains(QLatin1Char('\r'))
            && !value.contains(QLatin1Char('\n'))) {
        return value;
    }
    value.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}

bool ResultService::submitImageSave(
    const ProcessRequest &request,
    DetectionResultSaveAction saveAction,
    const QImage &annotatedImage) const
{
    if (!m_imageSaveService) {
        qCCritical(logRuntime).noquote()
                << "event=image_save.submit_failed reason=service_unavailable";
        return false;
    }
    const ResultSaveOptions &options = request.saveOptions;
    if (options.layout == ResultSaveLayout::AnnotatedAndRaw
            && options.rootDirectory.trimmed().isEmpty()) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=image_save.submit_failed reason=empty_directory");
        return false;
    }

    const QString format = normalizedFormat(options.format);
    const QString resultName = resultDirectoryName(saveAction);
    ImageSaveTask task;
    task.productKey = request.completion.frame->productKey;
    bool annotatedImageMissing = false;
    if (options.layout == ResultSaveLayout::RawOnly) {
        task.items.push_back(saveItem(
            QImage(),
            request.completion.frame,
            options.rootDirectory + QStringLiteral("/") + resultName,
            QDateTime::currentDateTime().toString(
                QStringLiteral("yyyyMMdd-hhmmss-zzz")),
            format));
    } else {
        const QString baseName = QDateTime::currentDateTime().toString(
                    QStringLiteral("yyyyMMdd-hhmmss.zzz"));
        const bool saveAnnotated = options.imageContentModeIndex == 0
                || options.imageContentModeIndex == 1;
        const bool saveRaw = options.imageContentModeIndex == 0
                || options.imageContentModeIndex == 2;
        if (saveAnnotated) {
            if (annotatedImage.isNull()) {
                annotatedImageMissing = true;
            } else {
                task.items.push_back(saveItem(
                    annotatedImage,
                    std::shared_ptr<const FrameData>(),
                    options.rootDirectory + QStringLiteral("/") + resultName,
                    baseName,
                    format));
            }
        }
        if (saveRaw) {
            task.items.push_back(saveItem(
                QImage(),
                request.completion.frame,
                options.rootDirectory + QStringLiteral("/")
                    + resultName + QStringLiteral("_raw"),
                baseName,
                format));
        }
    }
    if (task.items.empty()) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=image_save.submit_failed reason=%1")
                   .arg(annotatedImageMissing
                        ? QStringLiteral("empty_annotated_image")
                        : QStringLiteral("no_items"));
        return false;
    }
    if (annotatedImageMissing) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=image_save.partial reason=empty_annotated_image items=%1")
                   .arg(static_cast<qulonglong>(task.items.size()));
    }
    const ImageSaveSubmitResult submitted =
            m_imageSaveService->submit(task);
    if (!submitted.isAccepted()) {
        qCWarning(logRuntime).noquote()
                << QStringLiteral(
                    "event=image_save.submit_failed status=%1 items=%2")
                   .arg(imageSaveSubmitStatusName(submitted.status))
                   .arg(static_cast<qulonglong>(task.items.size()));
        return false;
    }
    return true;
}

bool ResultService::requestPlc(
    DetectionPlcAction action,
    const ProductKey &productKey)
{
    if (action == DetectionPlcAction::NoRequest
            || !productKey.isValid()) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_runConfiguration.plcOutputEnabled) {
            return false;
        }
    }
    if (!m_runtime.isPlcConnected()) {
        enterPlcFault(QStringLiteral(
            "PLC结果输出前检测到连接已断开。"));
        return false;
    }

    const std::uint8_t value = action == DetectionPlcAction::RequestOk
            ? static_cast<std::uint8_t>(0)
            : static_cast<std::uint8_t>(49);
    const PlcOperationResult result = m_runtime.writePlcResultValue(value);
    if (!result.isSuccess()) {
        enterPlcFault(QStringLiteral(
            "PLC结果输出值%1失败，错误码%2。")
            .arg(static_cast<int>(value))
            .arg(result.nativeErrorCode));
        return false;
    }
    if (action == DetectionPlcAction::RequestNg) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pendingPlcResetProducts.push_back(productKey);
        }
        if (QThread::currentThread() == thread()) {
            m_plcResetTimer.start(100);
        } else {
            QMetaObject::invokeMethod(
                        this,
                        [this]() { m_plcResetTimer.start(100); },
                        Qt::QueuedConnection);
        }
    }
    return true;
}

void ResultService::resetPlcPulse()
{
    std::vector<ProductKey> pending;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pending.swap(m_pendingPlcResetProducts);
    }
    if (pending.empty()) {
        return;
    }
    if (!m_runtime.isPlcConnected()) {
        enterPlcFault(QStringLiteral("PLC NG脉冲复位前连接已断开。"));
        return;
    }
    const PlcOperationResult result = m_runtime.writePlcResultValue(0);
    if (!result.isSuccess()) {
        enterPlcFault(QStringLiteral(
            "PLC NG脉冲复位失败，错误码%1。")
            .arg(result.nativeErrorCode));
    }
}

void ResultService::enterPlcFault(const QString &diagnostic)
{
    m_runtime.enterFault(
                InspectionFaultReason::PlcDisconnected,
                diagnostic);
}

void ResultService::handleCompletion(
    const DetectionCompletion &completion)
{
    const DetectionCompletion accepted = acceptCompletion(completion);
    if (!accepted.isValid()) {
        return;
    }

    ProcessRequest request;
    request.completion = accepted;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        request.saveOptions = m_runConfiguration.saveOptions;
    }
    request.saveOptions.layout = accepted.result.saveRawOnly
            ? ResultSaveLayout::RawOnly
            : ResultSaveLayout::AnnotatedAndRaw;
    process(request);
}
