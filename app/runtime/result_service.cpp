// 文件作用：本文件用于作为唯一检测结果入口，统一完成去重、统计、PLC、存图和界面呈现。
// 主要职责：作为唯一检测结果入口，统一完成去重、统计、PLC、存图和界面呈现。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/result_service.h"

#include "contracts/detection_mode.h"
#include "runtime/inspection_presentation_renderer.h"
#include "runtime/inspection_runtime.h"
#include "runtime/result_export_client.h"
#include "system_support/logging/log_categories.h"

#include <QDateTime>
#include <QDir>
#include <QMetaObject>
#include <QThread>

#include <chrono>

namespace {

// 函数说明：normalizedFormat 函数校验、转换或恢复对应数据。
QString normalizedFormat(QString format)
{
    format = format.trimmed();
    if (format.startsWith(QStringLiteral("."))) {
        format.remove(0, 1);
    }
    return format.isEmpty() ? QStringLiteral("png") : format.toLower();
}

// 函数说明：resultDirectoryName 函数实现名称所表示的处理步骤。
QString resultDirectoryName(DetectionResultSaveAction action)
{
    return action == DetectionResultSaveAction::SaveNg
            ? QStringLiteral("ng")
            : QStringLiteral("ok");
}

// 函数说明：saveItem 函数保存或发布对应的数据和资源。
ImageSaveItem saveItem(
    const QImage &image,
    const std::shared_ptr<const FrameData> &frame,
    const QString &directoryPath,
    const QString &baseName,
    const QString &format,
    int quality)
{
    ImageSaveItem item;
    item.image = image;
    item.frame = frame;
    item.filePath = QDir(directoryPath).filePath(
                baseName + QStringLiteral(".") + format);
    item.format = format.toUpper().toLatin1();
    item.quality = quality;
    return item;
}

// 函数说明：verdictStyle 函数实现名称所表示的处理步骤。
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

// 函数说明：ResultService 构造函数创建组件并初始化其依赖和初始状态。
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
    connect(m_imageSaveService.get(),
            &ImageSaveService::taskFailed,
            this,
            [this](quint64 totalFailed, const QString &latestError) {
        m_runtime.publishImageSaveFailure(totalFailed, latestError);
    },
    Qt::QueuedConnection);
}

// 函数说明：~ResultService 析构函数按生命周期要求释放组件持有的资源。
ResultService::~ResultService()
{
    shutdown();
}

// 函数说明：configureRun 函数更新或应用对应的配置和状态。
void ResultService::configureRun(
    const ResultServiceRunConfiguration &configuration)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_runConfiguration = configuration;
    std::queue<DelayedNgRequest> empty;
    m_delayedNgRequests.swap(empty);
    m_pendingPlcResetProducts.clear();
}

// 函数说明：requiresPlcForRun 函数实现名称所表示的处理步骤。
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

// 函数说明：statistics 函数实现名称所表示的处理步骤。
DetectionResultStatistics ResultService::statistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics;
}

// 函数说明：abnormalStatistics 函数实现名称所表示的处理步骤。
DetectionAbnormalStatistics ResultService::abnormalStatistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_abnormalStatistics;
}

// 函数说明：totalCount 函数校验、转换或恢复对应数据。
int ResultService::totalCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics.totalCount;
}

// 函数说明：ngCount 函数实现名称所表示的处理步骤。
int ResultService::ngCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statistics.ngCount;
}

// 函数说明：pendingDelayedNgCount 函数实现名称所表示的处理步骤。
int ResultService::pendingDelayedNgCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_delayedNgRequests.size());
}

// 函数说明：resetStatistics 函数停止流程、清理状态或释放对应资源。
void ResultService::resetStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statistics = DetectionResultStatistics();
}

// 函数说明：resetAbnormalStatistics 函数停止流程、清理状态或释放对应资源。
void ResultService::resetAbnormalStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_abnormalStatistics = DetectionAbnormalStatistics();
}

// 函数说明：resetNgCount 函数停止流程、清理状态或释放对应资源。
void ResultService::resetNgCount()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_statistics.ngCount = 0;
}

// 函数说明：clearPendingDelayedNgRequests 函数停止流程、清理状态或释放对应资源。
void ResultService::clearPendingDelayedNgRequests()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::queue<DelayedNgRequest> empty;
    m_delayedNgRequests.swap(empty);
}

// 函数说明：recordSystemFault 函数实现名称所表示的处理步骤。
void ResultService::recordSystemFault()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_abnormalStatistics.systemFaultCount;
        std::queue<DelayedNgRequest> empty;
        m_delayedNgRequests.swap(empty);
    }
}

// 函数说明：recordUnconfirmedProducts 函数实现名称所表示的处理步骤。
void ResultService::recordUnconfirmedProducts(int count)
{
    if (count <= 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_abnormalStatistics.unconfirmedProductCount +=
            static_cast<quint64>(count);
}

// 函数说明：recordPostFaultDroppedFrame 函数实现名称所表示的处理步骤。
void ResultService::recordPostFaultDroppedFrame()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_abnormalStatistics.postFaultDroppedFrameCount;
}

// 函数说明：shutdown 函数实现名称所表示的处理步骤。
void ResultService::shutdown()
{
    m_plcResetTimer.stop();
    if (m_imageSaveService) {
        m_imageSaveService->shutdown();
        m_imageSaveService.reset();
    }
}

bool ResultService::resultExportEnabled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_runConfiguration.resultExportEnabled;
}

// 函数说明：acceptCompletion 函数实现名称所表示的处理步骤。
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

// 函数说明：imageSaveActionFor 函数实现名称所表示的处理步骤。
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

// 函数说明：consumeDueDelayedNgRequest 函数执行对应事件或业务处理。
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

// 函数说明：process 函数执行对应事件或业务处理。
void ResultService::process(const ProcessRequest &request)
{
    if (!request.completion.isValid()) {
        return;
    }

    if (!m_runtime.claimResult(request.completion.frame->productKey)) {
        return;
    }

    const ProductKey productKey = request.completion.frame->productKey;
    bool exportEnabled = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        exportEnabled = m_runConfiguration.resultExportEnabled;
    }
    ResultExportRecord exportRecord;
    if (exportEnabled) {
        exportRecord.id = productKey.runId
                + QStringLiteral(":")
                + QString::number(productKey.sequence);
        exportRecord.eventTimeUtc = QDateTime::currentDateTimeUtc();
        exportRecord.overallOk = request.completion.result.verdict
                == AlgorithmVerdict::Ok;
        exportRecord.qrContent = exportRecord.overallOk
                ? request.completion.result.qrContent
                : QString();
    }
    if (!m_runtime.finalizeResultClaim(productKey)) {
        m_runtime.enterFault(
                    InspectionFaultReason::RuntimeInvariantViolation,
                    QStringLiteral("产品结果正式认领状态提交失败。"));
        return;
    }
    if (exportEnabled) {
        m_runtime.resultExportClient().enqueue(exportRecord);
    }

    ProductKey delayedProduct;
    if (consumeDueDelayedNgRequest(&delayedProduct)) {
        requestPlc(DetectionPlcAction::RequestNg, delayedProduct);
    }

    const DetectionResultSaveAction saveAction = imageSaveActionFor(
                request.completion.result.verdict);
    DetectionPlcAction plcAction = DetectionPlcAction::NoRequest;
    DetectionResultStatistics statistics;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_statistics.totalCount;
        if (request.completion.result.verdict == AlgorithmVerdict::Ok) {
            plcAction = DetectionPlcAction::RequestOk;
        } else {
            ++m_statistics.ngCount;
            if (m_runConfiguration.delayedNgOffset == 0) {
                plcAction = DetectionPlcAction::RequestNg;
            } else {
                DelayedNgRequest delayed;
                delayed.dueTotalCount = m_statistics.totalCount
                        + m_runConfiguration.delayedNgOffset;
                delayed.productKey = request.completion.frame->productKey;
                m_delayedNgRequests.push(delayed);
            }
        }
        statistics = m_statistics;
    }

    InspectionPresentation presentation;
    presentation.image =
            InspectionPresentationRenderer::renderDetectionFrame(
                request.completion.frame->originalImage,
                request.completion.result.overlay);
    presentation.verdictStyle = verdictStyle(
                request.completion.result.verdict);
    presentation.verdictText = presentation.verdictStyle
            == DetectionVerdictViewStyle::Correct
            ? QStringLiteral("正确")
            : QStringLiteral("错误");
    presentation.recognitionText =
            request.completion.result.hasPresentationText
            ? request.completion.result.presentationText
            : QString();
    presentation.updatesTemplateName =
            request.completion.result.updatesTemplateName;
    presentation.templateName = request.completion.result.templateName;

    const bool imageSaveRequested =
            saveAction != DetectionResultSaveAction::DoNotSave;
    if (imageSaveRequested) {
        submitImageSave(request, saveAction, presentation.image);
    }

    presentation.statistics = statistics;
    if (m_runtime.state() != InspectionRuntimeState::Fault) {
        requestPlc(plcAction, request.completion.frame->productKey);
    }
    const double processingElapsedMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now()
                - request.completion.frame->processingStartedAt).count();
    presentation.elapsedText = QStringLiteral("检测耗时 %1 ms")
            .arg(processingElapsedMs, 0, 'f', 2);
    m_runtime.publishPresentation(presentation);
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

// 函数说明：submitImageSave 函数执行对应事件或业务处理。
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
            format,
            options.quality));
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
                m_runtime.publishImageSaveFailure(
                            0,
                            QStringLiteral("未采集到标注图像。"));
            } else {
                task.items.push_back(saveItem(
                    annotatedImage,
                    std::shared_ptr<const FrameData>(),
                    options.rootDirectory + QStringLiteral("/") + resultName,
                    baseName,
                    format,
                    options.quality));
            }
        }
        if (saveRaw) {
            task.items.push_back(saveItem(
                QImage(),
                request.completion.frame,
                options.rootDirectory + QStringLiteral("/")
                    + resultName + QStringLiteral("_raw"),
                baseName,
                format,
                options.quality));
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

// 函数说明：requestPlc 函数实现名称所表示的处理步骤。
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

// 函数说明：resetPlcPulse 函数停止流程、清理状态或释放对应资源。
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

// 函数说明：enterPlcFault 函数实现名称所表示的处理步骤。
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
