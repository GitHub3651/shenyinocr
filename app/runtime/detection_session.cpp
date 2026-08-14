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
    m_acceptedProductSequence = 0;
    m_completedProductCount = 0;
    m_lastCompletedProductSequence = 0;
    m_acceptedFrames.clear();
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
    return m_completedProductCount;
}

quint64 DetectionSession::acceptedProductCount() const
{
    return m_acceptedProductSequence;
}

std::shared_ptr<const FrameData> DetectionSession::acceptFrame(
    const cv::Mat &image,
    quint64 frameNumber,
    int cameraIndex,
    const QDateTime &timestampUtc)
{
    if (image.empty()) {
        return std::shared_ptr<const FrameData>();
    }
    if (!isActive()) {
        begin();
    }

    ProductKey productKey;
    productKey.runId = m_runId;
    productKey.sequence = ++m_acceptedProductSequence;
    const std::shared_ptr<const FrameData> frame = makeFrameData(
                productKey,
                frameNumber > 0 ? frameNumber : productKey.sequence,
                cameraIndex,
                timestampUtc.isValid()
                ? timestampUtc
                : QDateTime::currentDateTimeUtc(),
                image);
    m_acceptedFrames[productKey.sequence] = frame;
    return frame;
}

DetectionCompletion DetectionSession::complete(
    const std::shared_ptr<const FrameData> &frame,
    const DetectionResult &result)
{
    if (!isActive()
            || !frame
            || !frame->productKey.isValid()
            || frame->originalImage.empty()
            || frame->productKey.runId != m_runId
            || frame->productKey.sequence > m_acceptedProductSequence
            || frame->productKey.sequence
               <= m_lastCompletedProductSequence) {
        return DetectionCompletion();
    }

    const std::map<quint64,
            std::weak_ptr<const FrameData>>::iterator accepted =
            m_acceptedFrames.find(frame->productKey.sequence);
    if (accepted == m_acceptedFrames.end()) {
        return DetectionCompletion();
    }
    const std::shared_ptr<const FrameData> acceptedFrame =
            accepted->second.lock();
    if (!acceptedFrame || acceptedFrame.get() != frame.get()) {
        return DetectionCompletion();
    }

    DetectionCompletion completion;
    completion.frame = frame;
    completion.result = result;
    m_acceptedFrames.erase(accepted);
    m_lastCompletedProductSequence = frame->productKey.sequence;
    ++m_completedProductCount;
    return completion;
}

DetectionCompletion DetectionSession::complete(
    const cv::Mat &image,
    const DetectionResult &result,
    quint64 frameNumber,
    int cameraIndex,
    const QDateTime &timestampUtc)
{
    return complete(
                acceptFrame(
                    image,
                    frameNumber,
                    cameraIndex,
                    timestampUtc),
                result);
}
