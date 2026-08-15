#pragma once

#include "runtime/detection_mode_worker_factory.h"
#include "ui/controllers/detection_completion_controller.h"

#include <QObject>

#include <functional>
#include <memory>

class InspectionRuntimeStartTransaction;

struct InspectionResultRunConfiguration
{
    bool plcOutputEnabled = false;
    int imageSaveModeIndex = 0;
    int delayedNgOffset = 0;
    DetectionCompletionSaveOptions saveOptions;
};

struct InspectionDetectionWorkerStartConfiguration
{
    int modeIndex = -1;
    std::vector<DetectionModeWorkerProfile> profiles;
    int stampThresholdPercent = 0;
    bool stampThresholdValid = false;
    QString targetText;
    std::vector<cv::Mat> stampTemplates;
    StampDetectionPipeline::OverlapDetectionFunction detectStampOverlap;
    IOcrEngine *ocrEngine = nullptr;
    TissueRecipeParameters tissueParameters;
    IBarcodeDecoder *barcodeDecoder = nullptr;
    InspectionResultRunConfiguration resultConfiguration;
};

struct InspectionResultCoordinatorCallbacks
{
    std::function<void(DetectionPlcAction, const ProductKey &)> requestPlc;
    std::function<void()> warnMissingAnnotatedImage;
    std::function<void(quint64, const QString &)> reportImageSaveFailure;
    std::function<void(bool)> clearLegacyPresentationState;
    std::function<void(const QString &)> storeLegacyRecognitionText;
    std::function<void()> showDetectionRoiWarning;
    std::function<void()> clearDetectionRoiWarning;
};

class InspectionResultCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit InspectionResultCoordinator(
        InspectionRuntimeController *runtimeController,
        const InspectionResultCoordinatorCallbacks &callbacks =
            InspectionResultCoordinatorCallbacks(),
        QObject *parent = nullptr);
    ~InspectionResultCoordinator() override;

    void bindView(const DetectionResultViewBindings &bindings);
    void configureRun(
        const InspectionResultRunConfiguration &configuration);
    bool requiresPlcForRun() const;
    DetectionModeWorkerConsumers workerConsumers();
    bool startDetectionWorker(
        InspectionRuntimeStartTransaction &startTransaction,
        const InspectionDetectionWorkerStartConfiguration &configuration,
        QString *errorMessage);

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
    void shutdown();

private:
    bool postUiWork(const UiCompletionMailbox::Work &work);
    DetectionCompletion acceptCompletion(
        const DetectionCompletion &completion,
        const char *modeName);
    DetectionCompletionProcessRequest baseRequest(
        const DetectionCompletion &completion) const;
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

    void clearLegacyPresentationState(bool clearImageLabelRects) const;

    InspectionRuntimeController *m_runtimeController = nullptr;
    InspectionResultCoordinatorCallbacks m_callbacks;
    InspectionResultRunConfiguration m_runConfiguration;
    DetectionResultPresenter m_presenter;
    std::unique_ptr<ImageSaveService> m_imageSaveService;
    std::unique_ptr<DetectionCompletionController> m_completionController;
};
