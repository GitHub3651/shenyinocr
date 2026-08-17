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

class InspectionRuntime;

enum class DetectionResultSaveAction
{
    DoNotSave,
    SaveOk,
    SaveNg
};

enum class DetectionPlcAction
{
    NoRequest,
    RequestOk,
    RequestNg
};

enum class ResultSaveLayout
{
    AnnotatedAndRaw,
    RawOnly
};

struct ResultSaveOptions
{
    ResultSaveLayout layout = ResultSaveLayout::AnnotatedAndRaw;
    QString rootDirectory;
    QString format = QStringLiteral("png");
    int quality = -1;
    int imageContentModeIndex = 0;
    bool saveNotEvaluatedAsNg = true;
};

struct ResultServiceRunConfiguration
{
    bool plcOutputEnabled = false;
    int imageSaveModeIndex = 0;
    int delayedNgOffset = 0;
    ResultSaveOptions saveOptions;
};

struct ResultServiceCallbacks
{
    std::function<void()> runtimeFaulted;
    std::function<void()> warnMissingAnnotatedImage;
    std::function<void(quint64, const QString &)> reportImageSaveFailure;
    std::function<void(bool)> clearPreviousOverlay;
    std::function<void()> showDetectionRoiWarning;
    std::function<void()> clearDetectionRoiWarning;
};

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
    struct DelayedNgRequest
    {
        int dueTotalCount = 0;
        ProductKey productKey;
    };

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
