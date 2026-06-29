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
void MyThread::receiveangle(int a) { angle1 = a; }
void MyThread::receivecolorchannel1(int c) { colorc1 = c; }
void MyThread::getCameraPtr(CMvCamera *camera) { cameraPtr = camera; }
void MyThread::getImagePtr(cv::Mat *image) { imagePtr = image; }
void MyThread::received(QString data) { receivedata = data; }

void MyThread::run() {
    if (!cameraPtr || !imagePtr) return;
    m_stopRequested.store(false);

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
                    emit signal_cleanlabel();
                    emit signal_sendTissueResult(detectionImage, result);
                    lastDetectionTime = now;
                }
            } else {
                cv::Mat displayImage = imagePtr->clone();
                if (m_tracking.load() && m_poseMatcher.isReady()) {
                    DetectionPose pose = m_poseMatcher.match(*imagePtr, initialDatePoly);
                    emit signal_boxesSelected(pose);
                    if (pose.valid) {
                        auto now = std::chrono::steady_clock::now();
                        int interval = receivedata.toInt();
                        if (interval <= 0) interval = 300;
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDetectionTime).count() >= interval) {
                            emit signal_cleanlabel(); //
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
    if (!m_trackingTemplate.empty()) {
        m_trackingTemplate.release();
    }
}
bool MyThread::CheckRisingEdge() { return false; /* 保持原接口 */ }
