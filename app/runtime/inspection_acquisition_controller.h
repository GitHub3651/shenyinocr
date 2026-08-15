#pragma once

#include "TrackingTypes.h"
#include "devices/camera/camera_device.h"
#include "runtime/inspection_acquisition_stop_coordinator.h"
#include "runtime/inspection_camera_recovery_transition.h"
#include "runtime/inspection_camera_start_transition.h"
#include "runtime/inspection_run_configuration.h"
#include "runtime/inspection_runtime_controller.h"

#include <QObject>

#include <functional>
#include <memory>
#include <vector>

class CameraThread;
class MyThread;

struct InspectionAcquisitionCallbacks
{
    std::function<bool()> suppressStreamingFrame;
    std::function<void(const cv::Mat &)> presentStreamingFrame;
    std::function<void(const DetectionPose &)> presentTrackingPose;
    std::function<void()> clearResultText;
    std::function<void(quint64, const cv::Mat &)> presentTemplatePreview;
    std::function<void(quint64, const QString &)> reportTemplatePreviewError;
    std::function<void()> softwareThreadFinished;
    std::function<void()> hardwareThreadFinished;
    std::function<void(InspectionFaultReason, const QString &)> enterFault;
};
class InspectionAcquisitionController : public QObject
{
    Q_OBJECT

public:
    explicit InspectionAcquisitionController(
        const std::shared_ptr<ICameraDevice> &cameraDevice,
        InspectionRuntimeController *runtimeController,
        const InspectionAcquisitionCallbacks &callbacks =
            InspectionAcquisitionCallbacks(),
        QObject *parent = nullptr);
    ~InspectionAcquisitionController() override;

    bool hasCamera() const;
    bool isSoftwareRunning() const;
    bool isHardwareRunning() const;
    bool hasRunningInspectionThread(bool templatePreviewActive) const;

    CameraOperationResult enumerateDevices(int *deviceCount);
    CameraOperationResult openDevice(int deviceIndex);
    CameraOperationResult closeCamera();
    CameraOperationResult setEnumValue(const char *key, unsigned int value);
    CameraOperationResult setFloatValue(const char *key, float value);
    CameraOperationResult getFloatValue(
        const char *key,
        CameraFloatValue *value);
    CameraOperationResult registerImageCallback();
    CameraOperationResult startGrabbing();
    void requestCameraStop();

    InspectionCameraStartResult applyCameraStart(
        InspectionAcquisitionKind acquisitionKind,
        float gain,
        const std::function<bool(QString *)> &applyExposure);
    InspectionCameraRecoveryResult recoverCamera(
        bool recoveryRequired,
        bool cameraWasOpen,
        const std::function<bool(QString *, QString *)> &applySavedExposure);

    void configureSoftwareWorker(
        const InspectionRunPlan &plan,
        const std::vector<WordTrackingProfile> &wordProfiles,
        const std::vector<cv::Point2f> &datePolygon,
        const cv::Rect2d &trackingBox,
        const cv::Mat &trackingTemplate);
    void configureHardwareWorker(
        const InspectionRunPlan &plan,
        const std::vector<WordTrackingProfile> &wordProfiles,
        const std::vector<cv::Point2f> &datePolygon,
        const cv::Rect2d &trackingBox,
        const cv::Mat &trackingTemplate);
    void applyThreadSettings(
        int angle,
        int colorChannel,
        const QString &delayText);

    bool startSoftwareWorker();
    bool startHardwareWorker();
    void requestSoftwareStop();
    void requestHardwareStop();
    bool waitForSoftware(unsigned long milliseconds);
    bool waitForHardware(unsigned long milliseconds);
    bool reinitializeSoftwareWorker();
    bool reinitializeHardwareWorker();
    void ensureWorkersReady();

    bool startTemplatePreview(
        quint64 sessionId,
        int angle,
        int colorChannel);
    bool stopTemplatePreview(
        quint64 invalidatedSessionId,
        int waitTimeMs);
    void disableTemplatePreview(quint64 invalidatedSessionId);

    InspectionAcquisitionStopResult stopInspection();
    void stopForApplicationExit(unsigned long waitTimeMs);
    void shutdown(unsigned long waitTimeMs = 3000);

    bool hasCurrentImage() const;
    cv::Mat currentImageClone() const;
    void replaceCurrentImage(const cv::Mat &image);

private:
    void createSoftwareWorker();
    void createHardwareWorker();
    void connectSoftwareWorker(MyThread *worker);
    void connectHardwareWorker(CameraThread *worker);
    bool streamingSuppressed() const;
    void queueStreamingFrame(const cv::Mat &image);
    void queueTrackingPose(const DetectionPose &pose);
    void submitSoftwareFrame(const cv::Mat &image);
    void submitSoftwarePositionedFrame(
        const cv::Mat &image,
        const DetectionPose &pose);
    void submitHardwareFrame(const cv::Mat &image);
    void submitHardwarePositionedFrame(
        const cv::Mat &image,
        const DetectionPose &pose);
    void enterQueueOverflowFault(
        const std::shared_ptr<const FrameData> &frame) const;

    std::shared_ptr<ICameraDevice> m_cameraDevice;
    InspectionRuntimeController *m_runtimeController = nullptr;
    InspectionAcquisitionCallbacks m_callbacks;
    MyThread *m_softwareWorker = nullptr;
    CameraThread *m_hardwareWorker = nullptr;
    std::unique_ptr<cv::Mat> m_imageBuffer;
    int m_angle = 0;
    int m_colorChannel = 0;
    QString m_delayText;
    bool m_shutdown = false;
};
