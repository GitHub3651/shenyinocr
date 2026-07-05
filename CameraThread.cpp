// CameraThread.cpp
#include "CameraThread.h"
#include <QDebug>

CameraThread::CameraThread(QObject *parent, CMvCamera *camera) :
    QThread(parent), m_pcMyCamera(camera), m_running(true), zhuizong(std::make_unique<Zhuizong>()), colorc(0)
{
    presetTrackingBox = cv::Rect2d(0, 0, 0, 0);
    usePresetBoxes = false;
    bypassTracking = false;
}

CameraThread::~CameraThread() { m_running = false; wait(); }

void CameraThread::setPresetBoxes(const std::vector<cv::Point2f>& datePoly, const cv::Rect2d& trackBox) {
    presetDatePoly = datePoly;
    presetTrackingBox = trackBox;
    usePresetBoxes = true;
}

void CameraThread::setBypassTracking(bool enabled) {
    bypassTracking = enabled;
}

void CameraThread::setWordTemplateTrackingProfiles(const std::vector<WordTrackingProfile>& profiles) {
    m_wordTrackingProfiles.clear();
    m_wordMultiTemplateMode = false;
    tracking = false;
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

    m_wordMultiTemplateMode = !m_wordTrackingProfiles.empty();
    qDebug() << "[WORD_MULTI_TEMPLATE] CameraThread tracking profiles ready:"
             << static_cast<int>(m_wordTrackingProfiles.size());
}

void CameraThread::clearWordTemplateTrackingProfiles() {
    for (WordTrackingState &state : m_wordTrackingProfiles) {
        state.matcher.clear();
        state.ready = false;
    }
    m_wordTrackingProfiles.clear();
    m_wordMultiTemplateMode = false;
}

void CameraThread::run() {
    m_running = true;
    m_stopRequested.store(false);

    std::unique_ptr<cv::Mat> image = std::make_unique<cv::Mat>();
    m_pcMyCamera->setnonblocking(true); //

    std::vector<cv::Point2f> initialDatePoly = presetDatePoly;
    cv::Rect2d initialTrackingBox = presetTrackingBox;

    tracking = !m_trackingTemplate.empty() && m_poseMatcher.isReady();
    bool needInitTracker = (usePresetBoxes && !presetDatePoly.empty() && m_trackingTemplate.empty());

    while (m_running && !m_stopRequested.load()) {
        if (m_pcMyCamera) {
            try {
                *image = m_pcMyCamera->timesGetImage(); //
                if (image->empty()) { msleep(10); continue; }

                // 图像预处理（旋转与颜色通道）
                if (angle2 == 1) cv::rotate(*image, *image, cv::ROTATE_90_CLOCKWISE);
                else if (angle2 == 2) cv::rotate(*image, *image, cv::ROTATE_90_COUNTERCLOCKWISE);
                else if (angle2 == 3) cv::rotate(*image, *image, cv::ROTATE_180);

                if (colorc > 0 && image->channels() >= 3) {
                    std::vector<cv::Mat> channels;
                    cv::split(*image, channels);
                    if (colorc == 1) *image = channels[2];
                    else if (colorc == 2) *image = channels[1];
                    else if (colorc == 3) *image = channels[0];
                }

                if (needInitTracker && !tracking) {
                    cv::Rect imageRect(0, 0, image->cols, image->rows);
                    cv::Rect trackBoxInt(initialTrackingBox.x, initialTrackingBox.y,
                                         initialTrackingBox.width, initialTrackingBox.height);

                    if ((trackBoxInt & imageRect) == trackBoxInt) {
                        m_trackingTemplate = (*image)(trackBoxInt).clone();
                        tracking = m_poseMatcher.init(m_trackingTemplate);
                        needInitTracker = false;
                        if (tracking) {
                            const cv::Point2f center(initialTrackingBox.x + initialTrackingBox.width / 2.0f,
                                                     initialTrackingBox.y + initialTrackingBox.height / 2.0f);
                            emit signal_boxesSelected(buildDetectionPose(center,
                                                                         cv::Size2f(m_trackingTemplate.cols, m_trackingTemplate.rows),
                                                                         initialDatePoly,
                                                                         0.0f,
                                                                         1.0f));
                            m_pcMyCamera->deferswitchtoblockingafternextframe(); //
                        }
                    } else {
                        needInitTracker = false;
                    }
                }

                if (bypassTracking) {
                    if (m_pcMyCamera->isImageReadyForMain()) {
                        cv::Mat detectionImage = image->clone();
                        TissueRollDetector detector;
                        auto detectStart = std::chrono::high_resolution_clock::now();
                        TissueRollResult result = detector.processImage(detectionImage);
                        auto detectEnd = std::chrono::high_resolution_clock::now();
                        result.processingTimeMs = static_cast<int>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(detectEnd - detectStart).count());
                        emit signal_cleanlabel();
                        emit signal_sendTissueResult(detectionImage, result);
                    }
                } else {
                    cv::Mat displayImage = image->clone();
                    if (m_wordMultiTemplateMode && !m_wordTrackingProfiles.empty()) {
                        DetectionPose bestPose;
                        QString bestName;
                        for (const WordTrackingState &state : m_wordTrackingProfiles) {
                            if (!state.ready) {
                                continue;
                            }

                            DetectionPose pose = state.matcher.match(*image, state.datePoly);
                            if (!pose.valid) {
                                continue;
                            }

                            pose.wordTemplateProfileIndex = state.profileIndex;
                            if (!bestPose.valid || pose.score > bestPose.score) {
                                bestPose = pose;
                                bestName = state.name;
                            }
                        }

                        emit signal_boxesSelected(bestPose);
                        if (bestPose.valid) {
                            qDebug() << "[WORD_MULTI_TEMPLATE] CameraThread selected profile:"
                                     << bestPose.wordTemplateProfileIndex
                                     << bestName
                                     << "score:" << bestPose.score;

                            if (m_pcMyCamera->isImageReadyForMain()) {
                                emit signal_cleanlabel(); //
                                emit signal_sendForDetection(image->clone(), bestPose);
                            }
                        }
                    } else if (tracking && m_poseMatcher.isReady()) {
                        DetectionPose pose = m_poseMatcher.match(*image, initialDatePoly);
                        emit signal_boxesSelected(pose);
                        if (pose.valid) {
                            if (m_pcMyCamera->isImageReadyForMain()) {
                                emit signal_cleanlabel(); //
                                emit signal_sendForDetection(image->clone(), pose);
                            }
                        } else if (initialTrackingBox.width > 0 && initialTrackingBox.height > 0) {
                            cv::rectangle(displayImage, initialTrackingBox, cv::Scalar(0, 0, 255), 4, 1);
                        }
                    }

                    emit signal_messImage(displayImage); //
                }

            } catch (...) { msleep(10); continue; }
        }

        // 线程睡眠逻辑
        int sleepTime = receivedata.toInt();
        if (sleepTime <= 0) sleepTime = 100;
        int totalSleep = 0;
        while (totalSleep < sleepTime && !m_stopRequested.load()) {
            msleep(10);
            totalSleep += 10;
        }
    }
}

void CameraThread::clearPresetBoxes() { usePresetBoxes = false; }
void CameraThread::received(QString data) { receivedata = data; }
void CameraThread::receiveangle1(int a) { angle2 = a; }
void CameraThread::receivecolorchannel(int c) { colorc = c; }
void CameraThread::stopTracking() {
    tracking = false;
    m_poseMatcher.clear();
    clearWordTemplateTrackingProfiles();
    if (!m_trackingTemplate.empty()) {
        m_trackingTemplate.release();
    }
}

void CameraThread::forceStop() {
    requestStop();
    if (!wait(5000)) qDebug() << "WARNING: Thread did not finish in 5 seconds";
}

void CameraThread::requestStop() {
    m_stopRequested.store(true);
    m_running = false;
    if (m_pcMyCamera) {
        try { m_pcMyCamera->requestStop(); } catch (...) {}
    }
}

bool CameraThread::CheckRisingEdge() {
    bool previousState = false, currentState = false, inputStatus;
    if (m_pcMyCamera->GetBoolValue("LineStatus", &inputStatus) != MV_OK) return false;
    currentState = (inputStatus == true);
    bool risingEdgeDetected = (!previousState && currentState);
    previousState = currentState;
    return risingEdgeDetected;
}

