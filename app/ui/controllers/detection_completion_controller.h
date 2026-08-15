#pragma once

#include "TrackingTypes.h"
#include "runtime/image_save_service.h"
#include "runtime/inspection_runtime_controller.h"
#include "ui/presenters/detection_result_presenter.h"

#include <QString>

#include <functional>

enum class DetectionCompletionSaveLayout
{
    AnnotatedAndRaw,
    RawOnly
};

struct DetectionCompletionSaveOptions
{
    DetectionCompletionSaveLayout layout =
            DetectionCompletionSaveLayout::AnnotatedAndRaw;
    QString rootDirectory;
    QString format = QStringLiteral("png");
    int quality = -1;
    int imageContentModeIndex = 0;
    bool saveNotEvaluatedAsNg = true;
};

struct DetectionCompletionProcessRequest
{
    DetectionCompletion completion;
    int imageSaveModeIndex = 0;
    int delayedNgOffset = 0;
    DetectionCompletionSaveOptions saveOptions;
    std::function<DetectionResultViewSnapshot()> preparePresentation;
    std::function<void(DetectionResultViewSnapshot *)>
            finalizePresentation;
};

struct DetectionCompletionControllerCallbacks
{
    std::function<void(DetectionPlcAction, const ProductKey &)> requestPlc;
    std::function<void()> warnMissingAnnotatedImage;
};

struct DetectionCompletionProcessOutcome
{
    bool resultRecorded = false;
    bool delayedNgRequested = false;
    bool imageSaveRequested = false;
    bool imageSaveSubmitted = false;
    bool presentationAccepted = false;
    DetectionPlcAction plcAction = DetectionPlcAction::NoRequest;
    DetectionResultStatistics statistics;
};

class DetectionCompletionController
{
public:
    DetectionCompletionController(
        InspectionRuntimeController *runtimeController,
        ImageSaveService *imageSaveService,
        DetectionResultPresenter *resultPresenter,
        const DetectionCompletionControllerCallbacks &callbacks =
            DetectionCompletionControllerCallbacks());

    DetectionCompletionProcessOutcome process(
        const DetectionCompletionProcessRequest &request);

private:
    bool submitImageSave(
        const DetectionCompletionProcessRequest &request,
        const DetectionResultHandlingOutcome &handlingOutcome,
        const QImage &annotatedImage) const;
    void requestPlc(
        DetectionPlcAction action,
        const ProductKey &productKey) const;

    InspectionRuntimeController *m_runtimeController = nullptr;
    ImageSaveService *m_imageSaveService = nullptr;
    DetectionResultPresenter *m_resultPresenter = nullptr;
    DetectionCompletionControllerCallbacks m_callbacks;
};
