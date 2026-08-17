#pragma once

#include "detection/positioning/detection_pose.h"
#include "detection/common/frame_preprocessor.h"
#include "detection/positioning/inspection_positioner.h"
#include "devices/camera/camera_device.h"
#include "runtime/capture_worker.h"
#include "runtime/inspection_runtime.h"
#include "runtime/inspection_run_configuration.h"

#include <QString>

#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

enum class InspectionCameraOpenIssue
{
    None,
    DeviceNotFound,
    DeviceOpenFailed,
    ExposureFailed,
    InitializationFailed
};

struct InspectionCameraParameterResult
{
    bool success = false;
    int minimumValue = 0;
    int maximumValue = 0;
    double actualValue = 0.0;
    int nativeErrorCode = 0;
    QString diagnostic;
};

struct InspectionCameraOpenResult
{
    InspectionCameraOpenIssue issue = InspectionCameraOpenIssue::None;
    int deviceCount = 0;
    int appliedExposure = 0;
    int exposureMinimum = 0;
    int exposureMaximum = 0;
    bool exposureAdjusted = false;
    QString adjustmentMessage;
    QString diagnostic;

    bool isSuccess() const
    {
        return issue == InspectionCameraOpenIssue::None;
    }
};

enum class InspectionCameraRecoveryIssue
{
    None,
    MissingCamera,
    ExposureRejected,
    InitializationFailed
};

struct InspectionCameraRecoveryResult
{
    InspectionCameraRecoveryIssue issue =
            InspectionCameraRecoveryIssue::None;
    bool recoveryAttempted = false;
    bool cameraOpen = false;
    QString adjustmentMessage;
    QString errorMessage;

    bool isRecovered() const
    {
        return issue == InspectionCameraRecoveryIssue::None;
    }
};

struct CameraCaptureStopResult
{
    bool wasRunning = false;
    bool stopped = true;

    bool allStopped() const
    {
        return stopped;
    }

    bool shouldRestoreCamera() const
    {
        return wasRunning;
    }
};

struct CameraSessionCaptureConfiguration
{
    InspectionRunPlan runPlan;
    FramePreprocessSettings framePreprocess;
    int minimumIntervalMs = 0;
    int exposure = 0;
    int gain = 0;
    std::vector<WordTrackingProfile> trackingProfiles;
    std::vector<cv::Point2f> singleDatePolygon;
    cv::Mat singleTrackingTemplate;
};

struct CameraSessionCallbacks
{
    std::function<void(const cv::Mat &)> streamingFrameReady;
    std::function<void(const DetectionPose &)> trackingPoseReady;
    std::function<void(quint64, const cv::Mat &)> previewFrameReady;
    std::function<void(quint64, const QString &)> previewFailed;
    std::function<void(bool)> captureStopped;
    std::function<void(InspectionFaultReason, const QString &)> enterFault;
};

class CameraSession
{
public:
    typedef std::function<bool(int, QString *)>
            PersistAdjustedExposure;

    CameraSession(
        const std::shared_ptr<ICameraDevice> &cameraDevice,
        InspectionRuntime *runtime);
    ~CameraSession();

    void setCallbacks(const CameraSessionCallbacks &callbacks);
    bool isOpen() const;
    bool isCapturing() const;

    InspectionCameraOpenResult openFirst(
        int savedExposure,
        const PersistAdjustedExposure &persistAdjustedExposure);
    void close();
    InspectionCameraParameterResult queryExposureRange();
    InspectionCameraParameterResult queryGainRange();
    InspectionCameraParameterResult applyExposure(int exposure);
    InspectionCameraParameterResult applyGain(int gain);

    bool prepareInspection(
        const CameraSessionCaptureConfiguration &configuration,
        QString *errorMessage);
    bool startInspection(QString *errorMessage);
    CameraCaptureStopResult stopInspection();
    InspectionCameraRecoveryResult restorePreviewReady(
        int savedExposure,
        const PersistAdjustedExposure &persistAdjustedExposure);

    bool startPreview(
        quint64 sessionId,
        const FramePreprocessSettings &settings,
        QString *errorMessage);
    void acknowledgePreviewFrame(quint64 sessionId);
    bool stopPreview();

    bool hasCurrentImage() const;
    cv::Mat currentImageClone() const;
    void replaceCurrentImage(const cv::Mat &image);

private:
    InspectionCameraParameterResult parameterResult(
        const CameraResult &result,
        bool exposure) const;
    InspectionCameraParameterResult applySavedExposure(
        int savedExposure,
        const PersistAdjustedExposure &persistAdjustedExposure,
        QString *adjustmentMessage);
    void handleFrame(const CameraFrame &frame);
    void handleCaptureError(CameraFrameStatus status, int nativeErrorCode);
    void handleCaptureStopped();
    void submitFrame(const cv::Mat &image, const DetectionPose &pose);
    CameraSessionCallbacks callbacksSnapshot() const;

    std::shared_ptr<ICameraDevice> m_cameraDevice;
    InspectionRuntime *m_runtime = nullptr;
    CaptureWorker m_captureWorker;
    CameraSessionCaptureConfiguration m_configuration;
    InspectionPositioner m_positioner;
    bool m_open = false;
    bool m_prepared = false;
    std::atomic<bool> m_preview{false};
    std::atomic<bool> m_intentionalStop{false};
    std::atomic<quint64> m_previewSessionId{0};
    std::atomic<bool> m_previewFramePending{false};
    mutable std::mutex m_mutex;
    CameraSessionCallbacks m_callbacks;
    cv::Mat m_currentImage;
};
