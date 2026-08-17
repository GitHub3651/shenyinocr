#pragma once

#include "DetectionModes.h"
#include "application/inspection_run_context.h"
#include "runtime/inspection_profile_snapshot.h"
#include "runtime/inspection_run_configuration.h"

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
    struct Callbacks
    {
        std::function<BarcodeRuntimeReadiness()>
                prepareBarcodeDecoder;
        std::function<bool(
            const InspectionStartExecutionCommand &,
            InspectionRuntimeStartTransaction &,
            QString *)> startInspection;
        std::function<void()> rollbackStart;
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
    bool reconcileFaultProducts(
        QString *summary,
        QString *errorMessage) const;

private:
    Callbacks m_callbacks;
};
