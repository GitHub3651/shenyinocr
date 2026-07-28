// mythread.cpp
#include "mythread.h"
#include <QDebug>
#include <chrono>

MyThread::MyThread(QObject *parent)
    : QThread{parent}, myImage(new QImage()), zhuizong(new Zhuizong),
      cameraPtr(nullptr), imagePtr(nullptr), angle1(0), colorc1(0),
      m_stopRequested(false), m_tracking(false)
{

    lastDetectionTime = std::chrono::steady_clock::now();
    presetTrackingBox = cv::Rect2d(0, 0, 0, 0);
    usePresetBoxes = false;
    bypassTracking = false;
}

MyThread::~MyThread() {
    if (isRunning()) {
        requestStop();
        wait(1000);
    }
    delete myImage;
    if (zhuizong) delete zhuizong;
}

void MyThread::stop() { m_stopRequested.store(true); m_tracking.store(false); }
void MyThread::requestStop() { m_stopRequested.store(true); m_tracking.store(false); }

void MyThread::setPresetBoxes(const std::vector<cv::Point2f>& datePoly, const cv::Rect2d& trackBox) {
    presetDatePoly = datePoly;
    presetTrackingBox = trackBox;
    usePresetBoxes = true;
}

void MyThread::clearPresetBoxes() { usePresetBoxes = false; }
void MyThread::setBypassTracking(bool enabled) { bypassTracking = enabled; }

void MyThread::setWordTemplateTrackingProfiles(const std::vector<WordTrackingProfile>& profiles) {
    m_wordTrackingProfiles.clear();
    m_wordTemplateProfileMode = false;
    m_tracking.store(false);
    m_poseMatcher.clear();
    if (!m_trackingTemplate.empty()) {
        m_trackingTemplate.release();
    }

    for (const WordTrackingProfile &profile : profiles) {
        if (profile.trackingTemplate.empty() || profile.datePoly.empty() || profile.profileIndex < 0) {
            continue;
        }

        WordTrackingState state;
        state.name = profile.name;
        state.profileIndex = profile.profileIndex;
        state.datePoly = profile.datePoly;
        state.ready = state.matcher.init(profile.trackingTemplate);
        if (state.ready) {
            m_wordTrackingProfiles.push_back(state);
        }
    }

    m_wordTemplateProfileMode = !m_wordTrackingProfiles.empty();
    qDebug() << "[WORD_TEMPLATE_PROFILE] MyThread tracking profiles ready:"
             << static_cast<int>(m_wordTrackingProfiles.size());
}

void MyThread::clearWordTemplateTrackingProfiles() {
    for (WordTrackingState &state : m_wordTrackingProfiles) {
        state.matcher.clear();
        state.ready = false;
    }
    m_wordTrackingProfiles.clear();
    m_wordTemplateProfileMode = false;
}

void MyThread::setTemplatePreviewMode(bool enabled, quint64 sessionId)
{
    m_templatePreviewSessionId.store(sessionId);
    m_templatePreviewMode.store(enabled);
    m_templatePreviewFramePending.store(false);
}

void MyThread::acknowledgeTemplatePreviewFrame(quint64 sessionId)
{
    if (m_templatePreviewSessionId.load() == sessionId) {
        m_templatePreviewFramePending.store(false);
    }
}

void MyThread::receiveangle(int a) { angle1 = a; }
void MyThread::receivecolorchannel1(int c) { colorc1 = c; }
void MyThread::getCameraPtr(CMvCamera *camera) { cameraPtr = camera; }
void MyThread::getImagePtr(cv::Mat *image) { imagePtr = image; }
void MyThread::received(QString data) { receivedata = data; }

void MyThread::runTemplatePreview(quint64 sessionId)
{
    int consecutiveFailures = 0;

    while (cameraPtr
           && !m_stopRequested.load()
           && m_templatePreviewMode.load()
           && m_templatePreviewSessionId.load() == sessionId) {
        try {
            // 丢弃进入预览前遗留的就绪标志，确保随后取得的是本次触发产生的新帧。
            cv::Mat discardedFrame;
            cameraPtr->takeImageForMainIfReady(discardedFrame);
            const uint64_t frameSequenceBefore =
                    cameraPtr->m_frameseq.load();

            const int triggerResult =
                    cameraPtr->CommandExecute("TriggerSoftware");
            if (triggerResult != MV_OK) {
                ++consecutiveFailures;
                if (consecutiveFailures >= 3) {
                    emit signal_templatePreviewError(
                                QStringLiteral(
                                    "连续3次执行相机软件触发失败，错误码：%1")
                                .arg(triggerResult),
                                sessionId);
                    break;
                }
                continue;
            }

            cv::Mat previewFrame;
            bool receivedNewFrame = false;
            const std::chrono::steady_clock::time_point waitStart =
                    std::chrono::steady_clock::now();

            while (!m_stopRequested.load()
                   && m_templatePreviewMode.load()
                   && m_templatePreviewSessionId.load() == sessionId) {
                if (cameraPtr->m_frameseq.load() > frameSequenceBefore
                        && cameraPtr->takeImageForMainIfReady(previewFrame)
                        && !previewFrame.empty()) {
                    receivedNewFrame = true;
                    break;
                }

                const qint64 waitedMs =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - waitStart).count();
                if (waitedMs >= 500) {
                    break;
                }
                msleep(2);
            }

            if (m_stopRequested.load()
                    || !m_templatePreviewMode.load()
                    || m_templatePreviewSessionId.load() != sessionId) {
                break;
            }

            if (!receivedNewFrame) {
                ++consecutiveFailures;
                if (consecutiveFailures >= 3) {
                    const uint64_t frameSequenceAfter =
                            cameraPtr->m_frameseq.load();
                    qWarning() << "[TEMPLATE_PREVIEW] frame timeout:"
                               << "sessionId=" << sessionId
                               << "frameSequenceBefore="
                               << frameSequenceBefore
                               << "frameSequenceAfter="
                               << frameSequenceAfter;
                    emit signal_templatePreviewError(
                                QStringLiteral(
                                    "连续3次等待相机图像超时，"
                                    "请检查相机连接和触发设置。"),
                                sessionId);
                    break;
                }
                continue;
            }

            consecutiveFailures = 0;

            if (angle1 == 1) {
                cv::rotate(previewFrame,
                           previewFrame,
                           cv::ROTATE_90_CLOCKWISE);
            } else if (angle1 == 2) {
                cv::rotate(previewFrame,
                           previewFrame,
                           cv::ROTATE_90_COUNTERCLOCKWISE);
            } else if (angle1 == 3) {
                cv::rotate(previewFrame,
                           previewFrame,
                           cv::ROTATE_180);
            }

            if (colorc1 > 0 && previewFrame.channels() >= 3) {
                std::vector<cv::Mat> channels;
                cv::split(previewFrame, channels);
                if (colorc1 == 1) {
                    previewFrame = channels[2];
                } else if (colorc1 == 2) {
                    previewFrame = channels[1];
                } else if (colorc1 == 3) {
                    previewFrame = channels[0];
                }
            }

            // 采集保持全速，但事件队列中最多保留一张待显示图像，避免UI积压旧帧。
            if (!m_templatePreviewFramePending.exchange(true)) {
                emit signal_templatePreviewImage(
                            previewFrame,
                            sessionId);
            }
        } catch (...) {
            ++consecutiveFailures;
            if (consecutiveFailures >= 3) {
                emit signal_templatePreviewError(
                            QStringLiteral(
                                "模板实时取景发生异常，"
                                "请重新打开相机后重试。"),
                            sessionId);
                break;
            }
        }
    }

    m_templatePreviewMode.store(false);
    m_templatePreviewFramePending.store(false);
}

void MyThread::run() {
    if (!cameraPtr || !imagePtr) return;
    m_stopRequested.store(false);

    if (m_templatePreviewMode.load()) {
        runTemplatePreview(m_templatePreviewSessionId.load());
        return;
    }

    m_tracking.store(!m_trackingTemplate.empty() && m_poseMatcher.isReady());

    std::vector<cv::Point2f> initialDatePoly = presetDatePoly;
    cv::Rect2d initialTrackingBox = presetTrackingBox;

    bool needInitTracker = (usePresetBoxes && !presetDatePoly.empty() && m_trackingTemplate.empty());
    lastDetectionTime = std::chrono::steady_clock::now(); //

    while (cameraPtr && !m_stopRequested.load()) {
        try {
            cameraPtr->CommandExecute("TriggerSoftware"); //
            *imagePtr = cameraPtr->timesGetImage(); //
            if (imagePtr->empty()) { msleep(10); continue; }

            // 图像旋转与通道处理
            if (angle1 == 1) cv::rotate(*imagePtr, *imagePtr, cv::ROTATE_90_CLOCKWISE);
            else if (angle1 == 2) cv::rotate(*imagePtr, *imagePtr, cv::ROTATE_90_COUNTERCLOCKWISE);
            else if (angle1 == 3) cv::rotate(*imagePtr, *imagePtr, cv::ROTATE_180);

            if (colorc1 > 0 && imagePtr->channels() >= 3) {
                std::vector<cv::Mat> channels;
                cv::split(*imagePtr, channels);
                if (colorc1 == 1) *imagePtr = channels[2];
                else if (colorc1 == 2) *imagePtr = channels[1];
                else if (colorc1 == 3) *imagePtr = channels[0];
            }

            if (needInitTracker && !m_tracking.load()) {
                cv::Rect imageRect(0, 0, imagePtr->cols, imagePtr->rows);
                cv::Rect trackBoxInt(initialTrackingBox.x, initialTrackingBox.y,
                                     initialTrackingBox.width, initialTrackingBox.height);
                if ((trackBoxInt & imageRect) == trackBoxInt) {
                    m_trackingTemplate = (*imagePtr)(trackBoxInt).clone();
                    m_tracking.store(m_poseMatcher.init(m_trackingTemplate));
                    needInitTracker = false;
                    if (m_tracking.load()) {
                        const cv::Point2f center(initialTrackingBox.x + initialTrackingBox.width / 2.0f,
                                                 initialTrackingBox.y + initialTrackingBox.height / 2.0f);
                        emit signal_boxesSelected(buildDetectionPose(center,
                                                                     cv::Size2f(m_trackingTemplate.cols, m_trackingTemplate.rows),
                                                                     initialDatePoly,
                                                                     0.0f,
                                                                     1.0f));
                    }
                } else { needInitTracker = false; }
            }

            if (bypassTracking) {
                auto now = std::chrono::steady_clock::now();
                int interval = receivedata.toInt();
                if (interval <= 0) interval = 300;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDetectionTime).count() >= interval) {
                    cv::Mat detectionImage = imagePtr->clone();
                    TissueRollDetector detector;
                    auto detectStart = std::chrono::high_resolution_clock::now();
                    TissueRollResult result = detector.processImage(detectionImage);
                    auto detectEnd = std::chrono::high_resolution_clock::now();
                    result.processingTimeMs = static_cast<int>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(detectEnd - detectStart).count());
                    emit signal_sendTissueResult(detectionImage, result);
                    lastDetectionTime = now;
                }
            } else {
                cv::Mat displayImage = imagePtr->clone();
                if (m_wordTemplateProfileMode && !m_wordTrackingProfiles.empty()) {
                    const auto trackingStart = std::chrono::steady_clock::now();
                    DetectionPose bestPose;
                    QString bestName;
                    cv::Mat sharedTrackingGray;
                    cv::Mat sharedTrackingSmallGray;
                    const bool framePrepared =
                            m_wordTrackingProfiles.front().matcher.prepareFrame(
                                *imagePtr,
                                &sharedTrackingGray,
                                &sharedTrackingSmallGray);
                    if (framePrepared) {
                        std::vector<DetectionPose> profilePoses(
                                    m_wordTrackingProfiles.size());
                        if (m_wordTrackingProfiles.size() == 1) {
                            const WordTrackingState &state =
                                    m_wordTrackingProfiles.front();
                            if (state.ready) {
                                profilePoses.front() =
                                        state.matcher.matchPrepared(
                                            sharedTrackingGray,
                                            sharedTrackingSmallGray,
                                            state.datePoly);
                            }
                        } else {
                            cv::parallel_for_(
                                    cv::Range(
                                        0,
                                        static_cast<int>(
                                            m_wordTrackingProfiles.size())),
                                    [&](const cv::Range &range) {
                            for (int i = range.start; i < range.end; ++i) {
                                const WordTrackingState &state =
                                        m_wordTrackingProfiles[
                                            static_cast<size_t>(i)];
                                if (!state.ready) {
                                    continue;
                                }
                                profilePoses[static_cast<size_t>(i)] =
                                        state.matcher.matchPrepared(
                                            sharedTrackingGray,
                                            sharedTrackingSmallGray,
                                            state.datePoly);
                            }
                            });
                        }

                        for (size_t i = 0;
                             i < m_wordTrackingProfiles.size();
                             ++i) {
                            const WordTrackingState &state =
                                    m_wordTrackingProfiles[i];
                            DetectionPose pose =
                                    profilePoses[i];
                            if (!pose.valid) {
                                continue;
                            }

                            pose.wordTemplateProfileIndex = state.profileIndex;
                            if (!bestPose.valid || pose.score > bestPose.score) {
                                bestPose = pose;
                                bestName = state.name;
                            }
                        }
                    }
                    bestPose.trackingElapsedMs =
                            std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - trackingStart).count();

                    emit signal_boxesSelected(bestPose);
                    if (bestPose.valid) {
                        qDebug() << "[WORD_TEMPLATE_PROFILE] MyThread selected profile:"
                                 << bestPose.wordTemplateProfileIndex
                                 << bestName
                                 << "score:" << bestPose.score;

                        auto now = std::chrono::steady_clock::now();
                        int interval = receivedata.toInt();
                        if (interval <= 0) interval = 300;
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDetectionTime).count() >= interval) {
                            emit signal_sendForDetection(imagePtr->clone(), bestPose); //
                            lastDetectionTime = now;
                        }
                    }
                } else if (m_tracking.load() && m_poseMatcher.isReady()) {
                    DetectionPose pose = m_poseMatcher.match(*imagePtr, initialDatePoly);
                    emit signal_boxesSelected(pose);
                    if (pose.valid) {
                        auto now = std::chrono::steady_clock::now();
                        int interval = receivedata.toInt();
                        if (interval <= 0) interval = 300;
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDetectionTime).count() >= interval) {
                            emit signal_sendForDetection(imagePtr->clone(), pose); //
                            lastDetectionTime = now;
                        }
                    } else if (initialTrackingBox.width > 0 && initialTrackingBox.height > 0) {
                        cv::rectangle(displayImage, initialTrackingBox, cv::Scalar(0, 0, 255), 4, 1);
                    }
                }

                emit signal_messImage(displayImage); //
            }

        } catch (...) { qDebug() << "Exception in run loop"; }
        msleep(100);
    }
}

void MyThread::startTracking() { m_tracking.store(m_poseMatcher.isReady()); lastDetectionTime = std::chrono::steady_clock::now(); }
void MyThread::stopTracking() {
    m_tracking.store(false);
    m_poseMatcher.clear();
    clearWordTemplateTrackingProfiles();
    if (!m_trackingTemplate.empty()) {
        m_trackingTemplate.release();
    }
}
bool MyThread::CheckRisingEdge() { return false; /* 保持原接口 */ }
