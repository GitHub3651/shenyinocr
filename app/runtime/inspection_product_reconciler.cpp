#include "runtime/inspection_product_reconciler.h"

void InspectionProductReconciler::beginRun(const QString &runId)
{
    m_runId = runId.trimmed();
    m_products.clear();
}

bool InspectionProductReconciler::noteAccepted(
    const ProductKey &productKey)
{
    if (!belongsToCurrentRun(productKey)) {
        return false;
    }

    return m_products.insert(std::make_pair(
                                 productKey.sequence,
                                 InspectionProductProgress::Accepted))
            .second;
}

bool InspectionProductReconciler::noteAlgorithmCompleted(
    const ProductKey &productKey)
{
    if (!belongsToCurrentRun(productKey)) {
        return false;
    }

    const std::map<quint64, InspectionProductProgress>::iterator product =
            m_products.find(productKey.sequence);
    if (product == m_products.end()
            || product->second
               != InspectionProductProgress::Accepted) {
        return false;
    }

    product->second = InspectionProductProgress::AlgorithmCompleted;
    return true;
}

bool InspectionProductReconciler::canRecordResult(
    const ProductKey &productKey) const
{
    if (!belongsToCurrentRun(productKey)) {
        return false;
    }

    const std::map<quint64, InspectionProductProgress>::const_iterator product =
            m_products.find(productKey.sequence);
    return product != m_products.end()
            && product->second
               == InspectionProductProgress::AlgorithmCompleted;
}

bool InspectionProductReconciler::noteResultRecorded(
    const ProductKey &productKey)
{
    if (!canRecordResult(productKey)) {
        return false;
    }

    const std::map<quint64, InspectionProductProgress>::iterator product =
            m_products.find(productKey.sequence);
    m_products.erase(product);
    return true;
}

std::vector<InspectionFaultProductAction>
InspectionProductReconciler::faultActions(bool plcWritable) const
{
    std::vector<InspectionFaultProductAction> actions;
    actions.reserve(m_products.size());

    const bool canRequestUniqueFallbackNg =
            plcWritable
            && m_products.size() == 1
            && m_products.begin()->second
               == InspectionProductProgress::Accepted;
    for (const std::pair<const quint64, InspectionProductProgress> &product
         : m_products) {
        InspectionFaultProductAction action;
        action.productKey.runId = m_runId;
        action.productKey.sequence = product.first;
        action.type = canRequestUniqueFallbackNg
                ? InspectionFaultProductActionType::RequestFallbackNg
                : InspectionFaultProductActionType::RecordUnconfirmed;
        actions.push_back(action);
    }
    return actions;
}

bool InspectionProductReconciler::resolve(
    const ProductKey &productKey,
    InspectionFaultProductResolution resolution)
{
    if (!belongsToCurrentRun(productKey)) {
        return false;
    }

    const std::map<quint64, InspectionProductProgress>::iterator product =
            m_products.find(productKey.sequence);
    if (product == m_products.end()) {
        return false;
    }
    if (resolution
            == InspectionFaultProductResolution::FallbackNgRequested
            && (m_products.size() != 1
                || product->second
                   != InspectionProductProgress::Accepted)) {
        return false;
    }

    m_products.erase(product);
    return true;
}

bool InspectionProductReconciler::hasUnresolvedProducts() const
{
    return !m_products.empty();
}

int InspectionProductReconciler::unresolvedProductCount() const
{
    return static_cast<int>(m_products.size());
}

bool InspectionProductReconciler::belongsToCurrentRun(
    const ProductKey &productKey) const
{
    return productKey.isValid()
            && !m_runId.isEmpty()
            && productKey.runId == m_runId;
}
