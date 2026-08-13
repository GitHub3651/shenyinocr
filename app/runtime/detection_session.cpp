#include "runtime/detection_session.h"

#include <QUuid>

namespace {
QString createRunId()
{
    return QUuid::createUuid()
            .toString(QUuid::WithoutBraces)
            .toLower();
}
}

DetectionSession::DetectionSession(
    const RunIdFactory &runIdFactory)
    : m_runIdFactory(runIdFactory)
{
}

QString DetectionSession::begin()
{
    m_runId = m_runIdFactory
            ? m_runIdFactory().trimmed()
            : QString();
    if (m_runId.isEmpty()) {
        m_runId = createRunId();
    }
    m_productSequence = 0;
    return m_runId;
}

bool DetectionSession::isActive() const
{
    return !m_runId.isEmpty();
}

QString DetectionSession::runId() const
{
    return m_runId;
}

quint64 DetectionSession::completedProductCount() const
{
    return m_productSequence;
}

DetectionCompletion DetectionSession::complete(
    const cv::Mat &image,
    const DetectionResult &result,
    quint64 frameNumber,
    int cameraIndex,
    const QDateTime &timestampUtc)
{
    if (!isActive()) {
        begin();
    }

    ProductKey productKey;
    productKey.runId = m_runId;
    productKey.sequence = ++m_productSequence;

    DetectionCompletion completion;
    completion.frame = makeFrameData(
                productKey,
                frameNumber > 0 ? frameNumber : productKey.sequence,
                cameraIndex,
                timestampUtc.isValid()
                ? timestampUtc
                : QDateTime::currentDateTimeUtc(),
                image);
    completion.result = result;
    return completion;
}
