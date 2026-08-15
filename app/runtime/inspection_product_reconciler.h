#pragma once

#include "TrackingTypes.h"

#include <map>
#include <utility>
#include <vector>

enum class InspectionProductProgress
{
    Accepted,
    AlgorithmCompleted
};

enum class InspectionFaultProductActionType
{
    RequestFallbackNg,
    RecordUnconfirmed
};

struct InspectionFaultProductAction
{
    ProductKey productKey;
    InspectionFaultProductActionType type =
            InspectionFaultProductActionType::RecordUnconfirmed;
};

enum class InspectionFaultProductResolution
{
    FallbackNgRequested,
    Unconfirmed
};

class InspectionProductReconciler
{
public:
    void beginRun(const QString &runId);

    bool noteAccepted(const ProductKey &productKey);
    bool noteAlgorithmCompleted(const ProductKey &productKey);
    bool canRecordResult(const ProductKey &productKey) const;
    bool noteResultRecorded(const ProductKey &productKey);

    std::vector<InspectionFaultProductAction> faultActions(
        bool plcWritable) const;
    bool resolve(
        const ProductKey &productKey,
        InspectionFaultProductResolution resolution);

    bool hasUnresolvedProducts() const;
    int unresolvedProductCount() const;

private:
    bool belongsToCurrentRun(const ProductKey &productKey) const;

    QString m_runId;
    std::map<quint64, InspectionProductProgress> m_products;
};
