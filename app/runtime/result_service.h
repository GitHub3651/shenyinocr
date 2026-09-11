#pragma once

#include "detection/common/detection_pose.h"
#include "runtime/image_save_service.h"
#include "contracts/inspection_presentation.h"
#include "runtime/detection_worker.h"

#include <QObject>
#include <QTimer>

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
    int imageContentModeIndex = 0;
};

struct ResultServiceRunConfiguration
{
    bool plcOutputEnabled = false;
    int imageSaveModeIndex = 0;
    int delayedNgOffset = 0;
    ResultSaveOptions saveOptions;
    bool barcodeCsvEnabled = false;
    QString barcodeCsvOutputDirectory;
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

    void configureRun(const ResultServiceRunConfiguration &configuration);
    bool requiresPlcForRun() const;
    DetectionWorker::CompletionConsumer completionConsumer();

    DetectionResultStatistics statistics() const;
    void resetStatistics();
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
    };

    DetectionCompletion acceptCompletion(
        const DetectionCompletion &completion);
    void handleCompletion(const DetectionCompletion &completion);
    void process(const ProcessRequest &request);
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
    bool appendBarcodeCsvResult(
        const DetectionResult &result,
        const QString &outputDirectory,
        QString *filePath,
        QString *errorMessage) const;
    static QString csvEscape(QString value);
    void resetPlcPulse();
    void enterPlcFault(const QString &diagnostic);

    InspectionRuntime &m_runtime;
    mutable std::mutex m_mutex;
    ResultServiceRunConfiguration m_runConfiguration;
    DetectionResultStatistics m_statistics;
    DetectionAbnormalStatistics m_abnormalStatistics;
    std::queue<DelayedNgRequest> m_delayedNgRequests;
    std::vector<ProductKey> m_pendingPlcResetProducts;
    std::unique_ptr<ImageSaveService> m_imageSaveService;
    QTimer m_plcResetTimer;
};
