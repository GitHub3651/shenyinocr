// 文件作用：本文件用于作为唯一检测结果入口，统一完成去重、统计、PLC、存图和界面呈现。
// 主要职责：作为唯一检测结果入口，统一完成去重、统计、PLC、存图和界面呈现。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "runtime/result_service.h"

#include "runtime/inspection_runtime.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMetaObject>
#include <QStringList>
#include <QThread>

#include <algorithm>

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

// 函数说明：profileTemplateName 函数实现名称所表示的处理步骤。
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
        ResultServiceCallbacks callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callbacks = m_callbacks;
        }
        if (callbacks.reportImageSaveFailure) {
            callbacks.reportImageSaveFailure(totalFailed, latestError);
        }
    },
    Qt::QueuedConnection);
}

// 函数说明：~ResultService 析构函数按生命周期要求释放组件持有的资源。
ResultService::~ResultService()
{
    shutdown();
}

// 函数说明：setCallbacks 函数更新或应用对应的配置和状态。
void ResultService::setCallbacks(const ResultServiceCallbacks &callbacks)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks = callbacks;
}

// 函数说明：bindView 函数实现名称所表示的处理步骤。
void ResultService::bindView(const InspectionPresentationViewBindings &bindings)
{
    std::lock_guard<std::mutex> lock(m_presentationMutex);
    m_presentationRenderer.bindView(bindings);
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

// 函数说明：pipelineConsumers 函数实现名称所表示的处理步骤。
PipelineResultConsumers ResultService::pipelineConsumers()
{
    PipelineResultConsumers consumers;
    consumers.tissue = [this](
        const DetectionCompletion &completion,
        const TissueRollResult &output) {
        handleTissueCompletion(completion, output);
    };
    consumers.ocr = [this](
        const DetectionCompletion &completion,
        const DetectionPose &pose) {
        handleOcrCompletion(completion, pose);
    };
    consumers.stamp = [this](
        const DetectionCompletion &completion,
        const StampDetectionWorkOutput &output) {
        handleStampCompletion(completion, output);
    };
    consumers.word = [this](
        const DetectionCompletion &completion,
        const WordDetectionWorkOutput &output) {
        handleWordCompletion(completion, output);
    };
    consumers.barcodeWord = [this](
        const DetectionCompletion &completion,
        const BarcodeWordDetectionWorkOutput &output) {
        handleBarcodeWordCompletion(completion, output);
    };
    return consumers;
}

// 函数说明：clear 函数停止流程、清理状态或释放对应资源。
void ResultService::clear()
{
    std::lock_guard<std::mutex> lock(m_presentationMutex);
    m_presentationRenderer.clear();
}

// 函数说明：clearTransientView 函数停止流程、清理状态或释放对应资源。
void ResultService::clearTransientView()
{
    std::lock_guard<std::mutex> lock(m_presentationMutex);
    m_presentationRenderer.clearTransientView();
}

// 函数说明：renderAndPresentFrame 函数执行对应事件或业务处理。
bool ResultService::renderAndPresentFrame(
    const cv::Mat &image,
    bool includeTissueOverlay)
{
    std::lock_guard<std::mutex> lock(m_presentationMutex);
    const QImage rendered = m_presentationRenderer.renderFrame(
                image, includeTissueOverlay);
    return !rendered.isNull()
            && m_presentationRenderer.presentFrame(rendered);
}

// 函数说明：presentPreviewFrame 函数执行对应事件或业务处理。
bool ResultService::presentPreviewFrame(
    const cv::Mat &image,
    bool tissueMode,
    bool productionRunning)
{
    if (image.empty() || (tissueMode && productionRunning)) {
        return false;
    }
    return renderAndPresentFrame(image, tissueMode);
}

// 函数说明：updatePose 函数更新或应用对应的配置和状态。
void ResultService::updatePose(const DetectionPose &pose)
{
    std::lock_guard<std::mutex> lock(m_presentationMutex);
    m_presentationRenderer.updatePose(pose);
}

// 函数说明：presentTotalAndNgCounts 函数执行对应事件或业务处理。
void ResultService::presentTotalAndNgCounts(int totalCount, int ngCount)
{
    std::lock_guard<std::mutex> lock(m_presentationMutex);
    m_presentationRenderer.presentTotalAndNgCounts(totalCount, ngCount);
}

// 函数说明：presentNgCount 函数执行对应事件或业务处理。
void ResultService::presentNgCount(int ngCount)
{
    std::lock_guard<std::mutex> lock(m_presentationMutex);
    m_presentationRenderer.presentNgCount(ngCount);
}

// 函数说明：lastPresentedProductKey 函数实现名称所表示的处理步骤。
ProductKey ResultService::lastPresentedProductKey() const
{
    std::lock_guard<std::mutex> lock(m_presentationMutex);
    return m_presentationRenderer.lastPresentedProductKey();
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
    ResultServiceCallbacks callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_abnormalStatistics.systemFaultCount;
        std::queue<DelayedNgRequest> empty;
        m_delayedNgRequests.swap(empty);
        callbacks = m_callbacks;
    }
    if (callbacks.runtimeFaulted) {
        callbacks.runtimeFaulted();
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

// 函数说明：postUiWork 函数实现名称所表示的处理步骤。
bool ResultService::postUiWork(const UiCompletionMailbox::Work &work)
{
    if (!m_runtime.submitUiCompletion(work)) {
        return false;
    }
    const bool posted = QMetaObject::invokeMethod(
                this,
                [this]() {
        if (!m_runtime.processOneUiCompletion()) {
            qDebug() << "[UI_COMPLETION] cancelled or empty work ignored";
        }
    },
    Qt::QueuedConnection);
    if (!posted) {
        m_runtime.cancelUiCompletion();
    }
    return posted;
}

// 函数说明：acceptCompletion 函数实现名称所表示的处理步骤。
DetectionCompletion ResultService::acceptCompletion(
    const DetectionCompletion &completion,
    const char *modeName)
{
    const DetectionCompletion accepted = m_runtime.complete(
                completion.frame, completion.result);
    if (!accepted.isValid()) {
        qDebug() << "[DETECTION_WORKER] stale" << modeName
                 << "completion ignored";
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
ResultServiceProcessOutcome ResultService::process(
    const ProcessRequest &request)
{
    ResultServiceProcessOutcome outcome;
    if (!request.completion.isValid()) {
        return outcome;
    }

    if (!m_runtime.claimResult(request.completion.frame->productKey)) {
        return outcome;
    }

    ProductKey delayedProduct;
    if (consumeDueDelayedNgRequest(&delayedProduct)) {
        requestPlc(DetectionPlcAction::RequestNg, delayedProduct);
    }

    InspectionPresentation presentation;
    {
        std::lock_guard<std::mutex> lock(m_presentationMutex);
        if (request.preparePresentation) {
            presentation = request.preparePresentation();
        }
        if (request.finalizePresentation) {
            request.finalizePresentation(&presentation);
        }
    }

    const DetectionResultSaveAction saveAction = imageSaveActionFor(
                request.completion.result.verdict);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_statistics.totalCount;
        if (request.completion.result.verdict == AlgorithmVerdict::Ok) {
            outcome.plcAction = DetectionPlcAction::RequestOk;
        } else {
            ++m_statistics.ngCount;
            if (m_runConfiguration.delayedNgOffset == 0) {
                outcome.plcAction = DetectionPlcAction::RequestNg;
            } else {
                DelayedNgRequest delayed;
                delayed.dueTotalCount = m_statistics.totalCount
                        + m_runConfiguration.delayedNgOffset;
                delayed.productKey = request.completion.frame->productKey;
                m_delayedNgRequests.push(delayed);
            }
        }
        outcome.statistics = m_statistics;
    }
    outcome.resultRecorded = true;

    outcome.imageSaveRequested =
            saveAction != DetectionResultSaveAction::DoNotSave
            && (request.completion.result.verdict
                != AlgorithmVerdict::NotEvaluated
                || request.saveOptions.saveNotEvaluatedAsNg);
    if (outcome.imageSaveRequested) {
        outcome.imageSaveSubmitted = submitImageSave(
                    request, saveAction, presentation.image);
    }

    presentation.productKey = request.completion.frame->productKey;
    presentation.statistics = outcome.statistics;
    outcome.presentationAccepted = postUiWork(
                [this, presentation, request]() {
        if (request.beforePresent) {
            request.beforePresent();
        }
        std::lock_guard<std::mutex> lock(m_presentationMutex);
        m_presentationRenderer.present(presentation);
    });

    if (m_runtime.state() != InspectionRuntimeState::Fault) {
        requestPlc(outcome.plcAction, request.completion.frame->productKey);
    }
    return outcome;
}

// 函数说明：submitImageSave 函数执行对应事件或业务处理。
bool ResultService::submitImageSave(
    const ProcessRequest &request,
    DetectionResultSaveAction saveAction,
    const QImage &annotatedImage) const
{
    if (!m_imageSaveService) {
        return false;
    }
    const ResultSaveOptions &options = request.saveOptions;
    if (options.layout == ResultSaveLayout::AnnotatedAndRaw
            && options.rootDirectory.trimmed().isEmpty()) {
        return false;
    }

    const QString format = normalizedFormat(options.format);
    const QString resultName = resultDirectoryName(saveAction);
    ImageSaveTask task;
    task.productKey = request.completion.frame->productKey;
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
                ResultServiceCallbacks callbacks;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    callbacks = m_callbacks;
                }
                if (callbacks.warnMissingAnnotatedImage) {
                    callbacks.warnMissingAnnotatedImage();
                }
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
    return !task.items.empty()
            && m_imageSaveService->submit(task).isAccepted();
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

// 函数说明：presentationElapsedMs 函数执行对应事件或业务处理。
qint64 ResultService::presentationElapsedMs(
    const DetectionCompletion &completion)
{
    qint64 elapsedMs = static_cast<qint64>(
                completion.result.elapsedMs + 0.5);
    if (!completion.frame || !completion.frame->timestampUtc.isValid()) {
        return elapsedMs;
    }
    return (std::max)(elapsedMs,
        completion.frame->timestampUtc.msecsTo(
            QDateTime::currentDateTimeUtc()));
}

// 函数说明：handleTissueCompletion 函数执行对应事件或业务处理。
void ResultService::handleTissueCompletion(
    const DetectionCompletion &completion,
    const TissueRollResult &output)
{
    const DetectionCompletion accepted = acceptCompletion(
                completion, "tissue");
    if (accepted.isValid()) {
        finalizeTissue(output, accepted);
    }
}

// 函数说明：handleOcrCompletion 函数执行对应事件或业务处理。
void ResultService::handleOcrCompletion(
    const DetectionCompletion &completion,
    const DetectionPose &pose)
{
    const DetectionCompletion accepted = acceptCompletion(completion, "OCR");
    if (accepted.isValid()
            && accepted.result.status != DetectionStatus::Cancelled) {
        finalizeOcr(pose, accepted);
    }
}

// 函数说明：handleStampCompletion 函数执行对应事件或业务处理。
void ResultService::handleStampCompletion(
    const DetectionCompletion &completion,
    const StampDetectionWorkOutput &output)
{
    const DetectionCompletion accepted = acceptCompletion(
                completion, "stamp");
    if (!accepted.isValid()) {
        return;
    }
    if (accepted.result.status == DetectionStatus::Cancelled) {
        ResultServiceCallbacks callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callbacks = m_callbacks;
        }
        if (callbacks.showDetectionRoiWarning) {
            postUiWork(callbacks.showDetectionRoiWarning);
        }
        return;
    }
    ResultServiceCallbacks callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks = m_callbacks;
    }
    if (callbacks.clearDetectionRoiWarning) {
        postUiWork(callbacks.clearDetectionRoiWarning);
    }
    finalizeStamp(output, accepted);
}

// 函数说明：handleWordCompletion 函数执行对应事件或业务处理。
void ResultService::handleWordCompletion(
    const DetectionCompletion &completion,
    const WordDetectionWorkOutput &output)
{
    const DetectionCompletion accepted = acceptCompletion(
                completion, "word");
    if (accepted.isValid()
            && accepted.result.status != DetectionStatus::Cancelled) {
        finalizeWord(output, accepted);
    }
}

// 函数说明：handleBarcodeWordCompletion 函数执行对应事件或业务处理。
void ResultService::handleBarcodeWordCompletion(
    const DetectionCompletion &completion,
    const BarcodeWordDetectionWorkOutput &output)
{
    const DetectionCompletion accepted = acceptCompletion(
                completion, "barcode-word");
    if (accepted.isValid()
            && accepted.result.status != DetectionStatus::Cancelled) {
        finalizeBarcodeWord(output, accepted);
    }
}

// 函数说明：finalizeTissue 函数实现名称所表示的处理步骤。
void ResultService::finalizeTissue(
    const TissueRollResult &output,
    const DetectionCompletion &completion)
{
    ProcessRequest request;
    request.completion = completion;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        request.saveOptions = m_runConfiguration.saveOptions;
    }
    const QString recognitionText = output.rollFound
            ? QStringLiteral("粗糙度：%1").arg(
                output.roll.roughnessScore, 0, 'f', 3)
            : QStringLiteral("粗糙度：--");
    request.preparePresentation = [this, completion, output,
        recognitionText]() {
        TissueRollPresentation roll;
        if (output.rollFound) {
            roll.center = output.roll.center;
            roll.outerAxes = output.roll.outerAxes;
            roll.innerCenter = output.roll.innerCenter;
            roll.innerAxes = output.roll.innerAxes;
        }
        m_presentationRenderer.installTissueRoll(roll, output.rollFound);
        InspectionPresentation presentation;
        presentation.image = m_presentationRenderer.renderFrame(
                    completion.frame->originalImage, true);
        presentation.verdictStyle = verdictStyle(completion.result.verdict);
        presentation.recognitionText = recognitionText;
        return presentation;
    };
    request.beforePresent = [this]() {
        clearPreviousOverlay(false);
    };
    request.finalizePresentation = [completion](
        InspectionPresentation *presentation) {
        presentation->elapsedText = QStringLiteral("检测耗时 %1 毫秒")
                .arg(presentationElapsedMs(completion));
    };
    process(request);
}

// 函数说明：finalizeOcr 函数实现名称所表示的处理步骤。
void ResultService::finalizeOcr(
    const DetectionPose &pose,
    const DetectionCompletion &completion)
{
    ProcessRequest request;
    request.completion = completion;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        request.saveOptions = m_runConfiguration.saveOptions;
    }
    request.saveOptions.layout = ResultSaveLayout::RawOnly;
    request.preparePresentation = [this, completion, pose]() {
        m_presentationRenderer.installDetectionResult(
                    completion.result, pose);
        InspectionPresentation presentation;
        presentation.image = m_presentationRenderer.renderFrame(
                    completion.frame->originalImage, false);
        presentation.verdictStyle = verdictStyle(completion.result.verdict);
        presentation.recognitionText = completion.result.recognizedText;
        return presentation;
    };
    request.beforePresent = [this]() { clearPreviousOverlay(false); };
    request.finalizePresentation = [completion](
        InspectionPresentation *presentation) {
        presentation->elapsedText = QStringLiteral("检测耗时 %1 毫秒")
                .arg(presentationElapsedMs(completion));
    };
    process(request);
}

// 函数说明：finalizeStamp 函数实现名称所表示的处理步骤。
void ResultService::finalizeStamp(
    const StampDetectionWorkOutput &output,
    const DetectionCompletion &completion)
{
    ProcessRequest request;
    request.completion = completion;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        request.saveOptions = m_runConfiguration.saveOptions;
    }
    request.saveOptions.saveNotEvaluatedAsNg = false;
    request.preparePresentation = [this, completion, output]() {
        m_presentationRenderer.installDetectionResult(
                    completion.result,
                    output.pose,
                    output.hasOverlapDetection
                    && !output.stampResult.overlapIsOk);
        InspectionPresentation presentation;
        presentation.image = m_presentationRenderer.renderFrame(
                    completion.frame->originalImage, false);
        presentation.verdictStyle = verdictStyle(completion.result.verdict);
        return presentation;
    };
    request.beforePresent = [this]() {
        clearPreviousOverlay(true);
    };
    request.finalizePresentation = [completion](
        InspectionPresentation *presentation) {
        presentation->elapsedText = QStringLiteral("检测耗时 %1 毫秒")
                .arg(presentationElapsedMs(completion));
    };
    process(request);
}

// 函数说明：finalizeWord 函数实现名称所表示的处理步骤。
void ResultService::finalizeWord(
    const WordDetectionWorkOutput &output,
    const DetectionCompletion &completion)
{
    bool updatesTemplateName = false;
    const QString templateName = profileTemplateName(
                output.templateName, output.pose, &updatesTemplateName);
    ProcessRequest request;
    request.completion = completion;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        request.saveOptions = m_runConfiguration.saveOptions;
    }
    request.saveOptions.saveNotEvaluatedAsNg = false;
    request.preparePresentation = [this, completion, output,
        updatesTemplateName, templateName]() {
        m_presentationRenderer.installDetectionResult(
                    completion.result, output.pose);
        InspectionPresentation presentation;
        presentation.image = m_presentationRenderer.renderFrame(
                    completion.frame->originalImage, false);
        presentation.verdictStyle = verdictStyle(completion.result.verdict);
        presentation.updatesTemplateName = updatesTemplateName;
        presentation.templateName = templateName;
        return presentation;
    };
    request.beforePresent = [this]() {
        clearPreviousOverlay(true);
    };
    request.finalizePresentation = [completion](
        InspectionPresentation *presentation) {
        presentation->elapsedText = QStringLiteral("检测耗时 %1 毫秒")
                .arg(presentationElapsedMs(completion));
    };
    process(request);
}

// 函数说明：finalizeBarcodeWord 函数实现名称所表示的处理步骤。
void ResultService::finalizeBarcodeWord(
    const BarcodeWordDetectionWorkOutput &output,
    const DetectionCompletion &completion)
{
    bool updatesTemplateName = false;
    const QString templateName = profileTemplateName(
                output.templateName, output.pose, &updatesTemplateName);
    QStringList lines;
    lines.append(QStringLiteral("二维码：%1").arg(output.barcodeState));
    if (!output.barcode.text.isEmpty()) {
        lines.append(QStringLiteral("二维码内容：%1").arg(
            output.barcode.text));
    }
    lines.append(QStringLiteral("日期：%1").arg(output.dateState));
    if ((!output.barcodeWordResult.barcodeIsReadable
         || !output.barcodeWordResult.dateDetectionExecuted)
            && !output.reason.trimmed().isEmpty()) {
        lines.append(QStringLiteral("原因：%1").arg(output.reason));
    }

    ProcessRequest request;
    request.completion = completion;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        request.saveOptions = m_runConfiguration.saveOptions;
    }
    request.saveOptions.saveNotEvaluatedAsNg = false;
    request.preparePresentation = [this, completion, output, lines,
        updatesTemplateName, templateName]() {
        m_presentationRenderer.installDetectionResult(
                    completion.result, output.pose);
        InspectionPresentation presentation;
        presentation.image = m_presentationRenderer.renderFrame(
                    completion.frame->originalImage, false);
        presentation.verdictStyle = verdictStyle(completion.result.verdict);
        presentation.recognitionText = lines.join(QStringLiteral("\n"));
        presentation.updatesTemplateName = updatesTemplateName;
        presentation.templateName = templateName;
        return presentation;
    };
    request.beforePresent = [this]() {
        clearPreviousOverlay(true);
    };
    request.finalizePresentation = [completion](
        InspectionPresentation *presentation) {
        presentation->elapsedText = QStringLiteral("检测耗时 %1 ms")
                .arg(static_cast<double>(presentationElapsedMs(completion)),
                     0, 'f', 2);
    };
    process(request);
}

// 函数说明：clearPreviousOverlay 函数停止流程、清理状态或释放对应资源。
void ResultService::clearPreviousOverlay(bool clearImageLabelRects) const
{
    ResultServiceCallbacks callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks = m_callbacks;
    }
    if (callbacks.clearPreviousOverlay) {
        callbacks.clearPreviousOverlay(clearImageLabelRects);
    }
}
