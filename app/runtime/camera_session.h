#pragma once

#include "contracts/camera_operation_result.h"
#include "detection/common/detection_pose.h"
#include "detection/common/frame_preprocessor.h"
#include "devices/camera/camera_device.h"
#include "runtime/capture_worker.h"
#include "runtime/inspection_runtime.h"

#include <QString>

#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

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
    bool hardwareTriggerEnabled = false;
    FramePreprocessSettings framePreprocess;
    float hardwareTriggerDelayMicroseconds = 0.0f;
    int exposure = 0;
    int gain = 0;
};

struct CameraSessionCallbacks
{
    std::function<void(const cv::Mat &)> previewFrameReady;
    std::function<void(const QString &)> previewFailed;
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

    CameraOpenResultDto openFirst(
        int savedExposure,
        const PersistAdjustedExposure &persistAdjustedExposure);
    void close();
    CameraParameterResultDto queryExposureRange();
    CameraParameterResultDto queryGainRange();
    CameraParameterResultDto applyExposure(int exposure);
    CameraParameterResultDto applyGain(int gain);

    bool prepareInspection(
        const CameraSessionCaptureConfiguration &configuration,
        QString *errorMessage);
    bool startInspection(QString *errorMessage);
    CameraCaptureStopResult stopInspection();
    CameraRecoveryResultDto restorePreviewReady(
        int savedExposure,
        const PersistAdjustedExposure &persistAdjustedExposure);

    bool startPreview(
        const FramePreprocessSettings &settings,
        QString *errorMessage);
    bool stopPreview();

    bool hasCurrentImage() const;
    cv::Mat currentImageClone() const;

private:
    CameraParameterResultDto parameterResult(
        const CameraResult &result,
        bool exposure) const;
    CameraParameterResultDto applySavedExposure(
        int savedExposure,
        const PersistAdjustedExposure &persistAdjustedExposure,
        QString *adjustmentMessage);
    void handleFrame(const CameraFrame &frame);
    void handleCaptureError(CameraFrameStatus status, int nativeErrorCode);
    void handleCaptureStopped();
    void submitFrame(const cv::Mat &image);
    CameraSessionCallbacks callbacksSnapshot() const;

    std::shared_ptr<ICameraDevice> m_cameraDevice;
    InspectionRuntime *m_runtime = nullptr;
    CaptureWorker m_captureWorker;
    CameraSessionCaptureConfiguration m_configuration;
    bool m_open = false;
    bool m_prepared = false;
    std::atomic<bool> m_preview{false};
    std::atomic<bool> m_intentionalStop{false};
    mutable std::mutex m_mutex;
    CameraSessionCallbacks m_callbacks;
    cv::Mat m_currentImage;
};
