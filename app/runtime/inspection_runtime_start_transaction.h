#pragma once

#include "runtime/inspection_runtime_controller.h"

class InspectionRuntimeStartTransaction
{
public:
    explicit InspectionRuntimeStartTransaction(
        InspectionRuntimeController &controller);
    ~InspectionRuntimeStartTransaction();

    InspectionRuntimeStartTransaction(
        const InspectionRuntimeStartTransaction &) = delete;
    InspectionRuntimeStartTransaction &operator=(
        const InspectionRuntimeStartTransaction &) = delete;

    bool begin();
    bool startDetectionWorker(
        int modeIndex,
        const std::shared_ptr<DetectionWorker> &worker);
    bool commit();
    void rollback();

    bool hasBegun() const;
    bool isCommitted() const;
    QString runId() const;

private:
    InspectionRuntimeController &m_controller;
    QString m_runId;
    bool m_hasBegun = false;
    bool m_committed = false;
};
