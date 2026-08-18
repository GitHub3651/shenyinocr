// 文件作用：本文件用于作为唯一检测结果入口，统一完成去重、统计、PLC、存图和界面呈现。
// 主要职责：作为唯一检测结果入口，统一完成去重、统计、PLC、存图和界面呈现。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/positioning/detection_pose.h"
#include "runtime/image_save_service.h"
#include "runtime/inspection_presentation.h"
#include "runtime/inspection_presentation_renderer.h"
#include "runtime/pipeline_registry.h"
#include "runtime/result_presentation_mailbox.h"

#include <QObject>
#include <QTimer>

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

// 组件说明：InspectionRuntime 组件封装对应业务职责和生命周期边界。
class InspectionRuntime;

// 组件说明：DetectionResultSaveAction 枚举列出该组件允许使用的稳定状态和选项。
enum class DetectionResultSaveAction
{
    DoNotSave,
    SaveOk,
    SaveNg
};

// 组件说明：DetectionPlcAction 枚举列出该组件允许使用的稳定状态和选项。
enum class DetectionPlcAction
{
    NoRequest,
    RequestOk,
    RequestNg
};

// 组件说明：ResultSaveLayout 枚举列出该组件允许使用的稳定状态和选项。
enum class ResultSaveLayout
{
    AnnotatedAndRaw,
    RawOnly
};

// 组件说明：ResultSaveOptions 组件集中描述相关配置、规则和运行参数。
struct ResultSaveOptions
{
    ResultSaveLayout layout = ResultSaveLayout::AnnotatedAndRaw;
    QString rootDirectory;
    QString format = QStringLiteral("png");
    int quality = -1;
    int imageContentModeIndex = 0;
    bool saveNotEvaluatedAsNg = true;
};

// 组件说明：ResultServiceRunConfiguration 组件集中描述相关配置、规则和运行参数。
struct ResultServiceRunConfiguration
{
    bool plcOutputEnabled = false;
    int imageSaveModeIndex = 0;
    int delayedNgOffset = 0;
    ResultSaveOptions saveOptions;
};

// 组件说明：ResultServiceCallbacks 数据结构集中传递该流程需要的只读数据或回调。
struct ResultServiceCallbacks
{
    std::function<void()> runtimeFaulted;
    std::function<void()> warnMissingAnnotatedImage;
    std::function<void(quint64, const QString &)> reportImageSaveFailure;
    std::function<void(bool)> clearPreviousOverlay;
    std::function<void()> showDetectionRoiWarning;
    std::function<void()> clearDetectionRoiWarning;
};

// 组件说明：ResultServiceProcessOutcome 数据结构保存一次操作的结果、状态和错误信息。
struct ResultServiceProcessOutcome
{
    bool resultRecorded = false;
    bool imageSaveRequested = false;
    bool imageSaveSubmitted = false;
    bool presentationAccepted = false;
    DetectionPlcAction plcAction = DetectionPlcAction::NoRequest;
    DetectionResultStatistics statistics;
};

// The only final-result transaction. It accepts one completed ProductKey,
// updates statistics once, submits at most one save task, performs the normal
// PLC contract and publishes one complete presentation.
class ResultService : public QObject
{
    Q_OBJECT

public:
    explicit ResultService(
        InspectionRuntime &runtime,
        QObject *parent = nullptr);
    ~ResultService() override;

    void setCallbacks(const ResultServiceCallbacks &callbacks);
    void bindView(const InspectionPresentationViewBindings &bindings);
    void configureRun(const ResultServiceRunConfiguration &configuration);
    bool requiresPlcForRun() const;
    PipelineResultConsumers pipelineConsumers();

    void clear();
    void clearTransientView();
    bool renderAndPresentFrame(
        const cv::Mat &image,
        bool includeTissueOverlay);
    bool presentPreviewFrame(
        const cv::Mat &image,
        bool tissueMode,
        bool productionRunning);
    void updatePose(const DetectionPose &pose);
    void presentTotalAndNgCounts(int totalCount, int ngCount);
    void presentNgCount(int ngCount);
    ProductKey lastPresentedProductKey() const;

    DetectionResultStatistics statistics() const;
    DetectionAbnormalStatistics abnormalStatistics() const;
    int totalCount() const;
    int ngCount() const;
    int pendingDelayedNgCount() const;
    void resetStatistics();
    void resetAbnormalStatistics();
    void resetNgCount();
    void clearPendingDelayedNgRequests();
    void recordSystemFault();
    void recordUnconfirmedProducts(int count);
    void recordPostFaultDroppedFrame();

    void shutdown();

private:
    // 组件说明：DelayedNgRequest 数据结构集中传递该流程需要的只读数据或回调。
    struct DelayedNgRequest
    {
        int dueTotalCount = 0;
        ProductKey productKey;
    };

    // 组件说明：ProcessRequest 数据结构集中传递该流程需要的只读数据或回调。
    struct ProcessRequest
    {
        DetectionCompletion completion;
        ResultSaveOptions saveOptions;
        std::function<InspectionPresentation()> preparePresentation;
        std::function<void(InspectionPresentation *)>
                finalizePresentation;
        std::function<void()> beforePresent;
    };

    bool postUiWork(const UiCompletionMailbox::Work &work);
    DetectionCompletion acceptCompletion(
        const DetectionCompletion &completion,
        const char *modeName);
    ResultServiceProcessOutcome process(const ProcessRequest &request);
    DetectionResultSaveAction imageSaveActionFor(
        AlgorithmVerdict verdict) const;
    bool consumeDueDelayedNgRequest(ProductKey *productKey);
    bool submitImageSave(
        const ProcessRequest &request,
        DetectionResultSaveAction saveAction,
        const QImage &annotatedImage) const;
    bool requestPlc(
        DetectionPlcAction action,
        const ProductKey &productKey);
    void resetPlcPulse();
    void enterPlcFault(const QString &diagnostic);

    static qint64 presentationElapsedMs(
        const DetectionCompletion &completion);
    void handleTissueCompletion(
        const DetectionCompletion &completion,
        const TissueRollResult &tissueResult);
    void handleOcrCompletion(
        const DetectionCompletion &completion,
        const DetectionPose &pose);
    void handleStampCompletion(
        const DetectionCompletion &completion,
        const StampDetectionWorkOutput &output);
    void handleWordCompletion(
        const DetectionCompletion &completion,
        const WordDetectionWorkOutput &output);
    void handleBarcodeWordCompletion(
        const DetectionCompletion &completion,
        const BarcodeWordDetectionWorkOutput &output);
    void finalizeTissue(
        const TissueRollResult &tissueResult,
        const DetectionCompletion &completion);
    void finalizeOcr(
        const DetectionPose &pose,
        const DetectionCompletion &completion);
    void finalizeStamp(
        const StampDetectionWorkOutput &output,
        const DetectionCompletion &completion);
    void finalizeWord(
        const WordDetectionWorkOutput &output,
        const DetectionCompletion &completion);
    void finalizeBarcodeWord(
        const BarcodeWordDetectionWorkOutput &output,
        const DetectionCompletion &completion);
    void clearPreviousOverlay(bool clearImageLabelRects) const;

    InspectionRuntime &m_runtime;
    mutable std::mutex m_mutex;
    mutable std::mutex m_presentationMutex;
    ResultServiceCallbacks m_callbacks;
    ResultServiceRunConfiguration m_runConfiguration;
    DetectionResultStatistics m_statistics;
    DetectionAbnormalStatistics m_abnormalStatistics;
    std::queue<DelayedNgRequest> m_delayedNgRequests;
    std::vector<ProductKey> m_pendingPlcResetProducts;
    InspectionPresentationRenderer m_presentationRenderer;
    std::unique_ptr<ImageSaveService> m_imageSaveService;
    QTimer m_plcResetTimer;
};
