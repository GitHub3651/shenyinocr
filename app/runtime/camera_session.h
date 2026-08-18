// 文件作用：本文件用于管理相机打开、参数下发、预览、正式采集和停止恢复的完整会话。
// 主要职责：管理相机打开、参数下发、预览、正式采集和停止恢复的完整会话。
// 模块位置：运行时层；负责编排采集、检测、结果、PLC和存图生命周期。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#pragma once

#include "detection/positioning/detection_pose.h"
#include "detection/common/frame_preprocessor.h"
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

// 组件说明：InspectionCameraOpenIssue 枚举列出该组件允许使用的稳定状态和选项。
enum class InspectionCameraOpenIssue
{
    None,
    DeviceNotFound,
    DeviceOpenFailed,
    ExposureFailed,
    InitializationFailed
};

// 组件说明：InspectionCameraParameterResult 数据结构保存一次操作的结果、状态和错误信息。
struct InspectionCameraParameterResult
{
    bool success = false;
    int minimumValue = 0;
    int maximumValue = 0;
    double actualValue = 0.0;
    int nativeErrorCode = 0;
    QString diagnostic;
};

// 组件说明：InspectionCameraOpenResult 数据结构保存一次操作的结果、状态和错误信息。
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

    // 函数说明：isSuccess 函数检查相关状态并返回判断结果。
    bool isSuccess() const
    {
        return issue == InspectionCameraOpenIssue::None;
    }
};

// 组件说明：InspectionCameraRecoveryIssue 枚举列出该组件允许使用的稳定状态和选项。
enum class InspectionCameraRecoveryIssue
{
    None,
    MissingCamera,
    ExposureRejected,
    InitializationFailed
};

// 组件说明：InspectionCameraRecoveryResult 数据结构保存一次操作的结果、状态和错误信息。
struct InspectionCameraRecoveryResult
{
    InspectionCameraRecoveryIssue issue =
            InspectionCameraRecoveryIssue::None;
    bool recoveryAttempted = false;
    bool cameraOpen = false;
    QString adjustmentMessage;
    QString errorMessage;

    // 函数说明：isRecovered 函数检查相关状态并返回判断结果。
    bool isRecovered() const
    {
        return issue == InspectionCameraRecoveryIssue::None;
    }
};

// 组件说明：CameraCaptureStopResult 数据结构保存一次操作的结果、状态和错误信息。
struct CameraCaptureStopResult
{
    bool wasRunning = false;
    bool stopped = true;

    // 函数说明：allStopped 函数检查相关状态并返回判断结果。
    bool allStopped() const
    {
        return stopped;
    }

    // 函数说明：shouldRestoreCamera 函数检查相关状态并返回判断结果。
    bool shouldRestoreCamera() const
    {
        return wasRunning;
    }
};

// 组件说明：CameraSessionCaptureConfiguration 组件集中描述相关配置、规则和运行参数。
struct CameraSessionCaptureConfiguration
{
    InspectionRunPlan runPlan;
    FramePreprocessSettings framePreprocess;
    float hardwareTriggerDelayMicroseconds = 0.0f;
    int exposure = 0;
    int gain = 0;
};

// 组件说明：CameraSessionCallbacks 数据结构集中传递该流程需要的只读数据或回调。
struct CameraSessionCallbacks
{
    std::function<void(quint64, const cv::Mat &)> previewFrameReady;
    std::function<void(quint64, const QString &)> previewFailed;
    std::function<void(bool)> captureStopped;
    std::function<void(InspectionFaultReason, const QString &)> enterFault;
};

// 组件说明：CameraSession 组件封装对应业务职责和生命周期边界。
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
    std::atomic<quint64> m_previewSessionId{0};
    std::atomic<bool> m_previewFramePending{false};
    mutable std::mutex m_mutex;
    CameraSessionCallbacks m_callbacks;
    cv::Mat m_currentImage;
};
