#pragma once

#include "DetectionModes.h"
#include "application/inspection_run_context.h"
#include "runtime/inspection_acquisition_stop_coordinator.h"
#include "runtime/inspection_camera_operations.h"
#include "runtime/inspection_camera_recovery_transition.h"
#include "runtime/inspection_profile_snapshot.h"
#include "runtime/inspection_run_configuration.h"
#include "system_support/settings/machine_settings.h"

#include <QString>

#include <functional>

class InspectionRuntimeStartTransaction;

struct BarcodeRuntimeReadiness
{
    bool ready = true;
    QString errorMessage;
};

struct InspectionStartExecutionCommand
{
    InspectionRunContext context;
    InspectionProfileSnapshot profileSnapshot;
    InspectionRunPlan runPlan;
    DetectionMode detectionMode = DetectionMode::Stamp;
    QString modeId;
    int modeIndex = -1;
    bool barcodeWordMode = false;
};

class InspectionRuntimePort
{
public:
    typedef std::function<bool(int, QString *)>
            PersistAdjustedExposure;

    struct Callbacks
    {
        std::function<BarcodeRuntimeReadiness()>
                prepareBarcodeDecoder;
        std::function<bool(
            const InspectionStartExecutionCommand &,
            InspectionRuntimeStartTransaction &,
            QString *)> startInspection;
        std::function<void()> rollbackStart;
        std::function<InspectionAcquisitionStopResult()>
                stopAcquisition;
        std::function<InspectionCameraRecoveryResult(
            bool,
            bool,
            const MachineSettings &,
            const PersistAdjustedExposure &)> recoverCamera;
        std::function<InspectionCameraOpenResult(
            const MachineSettings &,
            const PersistAdjustedExposure &)> openCamera;
        std::function<void()> closeCamera;
        std::function<bool(QString *, QString *)>
                reconcileFaultProducts;
    };

    void bind(const Callbacks &callbacks);

    BarcodeRuntimeReadiness prepareBarcodeDecoder() const;
    bool startInspection(
        const InspectionStartExecutionCommand &command,
        InspectionRuntimeStartTransaction &transaction,
        QString *errorMessage) const;
    void rollbackStart() const;
    InspectionAcquisitionStopResult stopAcquisition() const;
    InspectionCameraRecoveryResult recoverCamera(
        bool recoveryRequired,
        bool cameraWasOpen,
        const MachineSettings &settings,
        const PersistAdjustedExposure &persistAdjustedExposure) const;
    InspectionCameraOpenResult openCamera(
        const MachineSettings &settings,
        const PersistAdjustedExposure &persistAdjustedExposure) const;
    void closeCamera() const;
    bool reconcileFaultProducts(
        QString *summary,
        QString *errorMessage) const;

private:
    Callbacks m_callbacks;
};
