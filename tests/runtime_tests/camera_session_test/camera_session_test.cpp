#include <QtTest>

#include "detection/common/frame_preprocessor.h"
#include "detection/positioning/inspection_positioner.h"
#include "devices/camera/camera_device.h"
#include "runtime/capture_worker.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace {

class CaptureFakeCamera : public ICameraDevice
{
public:
    CameraResult enumerate(int *deviceCount) override
    {
        if (deviceCount) {
            *deviceCount = 1;
        }
        return CameraResult();
    }

    CameraResult openFirst() override
    {
        return CameraResult();
    }

    CameraResult applySettings(const CameraSettings &) override
    {
        CameraResult result;
        result.exposureRange.minimum = 100.0f;
        result.exposureRange.maximum = 10000.0f;
        result.exposureRange.current = 800.0f;
        result.gainRange.minimum = 0.0f;
        result.gainRange.maximum = 24.0f;
        result.gainRange.current = 1.0f;
        return result;
    }

    CameraResult setTriggerMode(CameraTriggerMode mode) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        triggerMode = mode;
        m_interrupted = false;
        return CameraResult();
    }

    CameraResult startGrabbing() override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interrupted = false;
        return CameraResult();
    }

    CameraResult triggerSoftware() override
    {
        ++softwareTriggerCount;
        if (autoFrameOnSoftware.load()) {
            enqueueFrame();
        }
        return CameraResult();
    }

    CameraFrameResult waitNextFrame(int timeoutMs) override
    {
        ++waitCount;
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait_for(
                    lock,
                    std::chrono::milliseconds(timeoutMs),
                    [this]() {
            return m_interrupted || !m_frames.empty();
        });
        if (m_interrupted) {
            CameraFrameResult result;
            result.status = CameraFrameStatus::Interrupted;
            return result;
        }
        if (m_frames.empty()) {
            return CameraFrameResult();
        }
        const CameraFrameResult result = m_frames.front();
        m_frames.pop_front();
        return result;
    }

    void interruptWait() override
    {
        ++interruptCount;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_interrupted = true;
        }
        m_condition.notify_all();
    }

    CameraResult stopGrabbing() override
    {
        return CameraResult();
    }

    CameraResult close() override
    {
        interruptWait();
        return CameraResult();
    }

    void enqueueFrame(int value = 1)
    {
        CameraFrameResult result;
        result.status = CameraFrameStatus::FrameReady;
        result.frame.sequence = ++m_sequence;
        result.frame.timestampUtc = QDateTime::currentDateTimeUtc();
        result.frame.image = cv::Mat(
                    8, 8, CV_8UC3, cv::Scalar(value, value, value));
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_frames.push_back(result);
        }
        m_condition.notify_one();
    }

    std::atomic<bool> autoFrameOnSoftware{false};
    std::atomic<int> softwareTriggerCount{0};
    std::atomic<int> waitCount{0};
    std::atomic<int> interruptCount{0};
    CameraTriggerMode triggerMode = CameraTriggerMode::Software;

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<CameraFrameResult> m_frames;
    bool m_interrupted = false;
    std::uint64_t m_sequence = 0;
};

cv::Mat patternedTemplate()
{
    cv::Mat value(20, 20, CV_8UC1);
    cv::RNG random(12345);
    random.fill(value, cv::RNG::UNIFORM, 0, 255);
    return value;
}

} // namespace

class CameraSessionTest : public QObject
{
    Q_OBJECT

private slots:
    void softwareTriggerPublishesFrames();
    void hardwareTriggerWaitsWithoutSoftwareCommand();
    void previewFramesRemainOutsideProductAdmission();
    void zeroAndNonZeroIntervalsAreHonored();
    void stopInterruptsWaitAndJoinsBeforeReturning();
    void preprocessorOwnsRotationAndChannelSelection();
    void positionerOwnsSingleAndProfileTracking();
};

void CameraSessionTest::softwareTriggerPublishesFrames()
{
    const std::shared_ptr<CaptureFakeCamera> camera(
                new CaptureFakeCamera);
    camera->autoFrameOnSoftware = true;
    CaptureWorker worker(camera);
    std::atomic<int> frames{0};
    std::atomic<bool> invalidFrame{false};
    CaptureWorkerCallbacks callbacks;
    callbacks.frameReady = [&frames, &invalidFrame](
            const CameraFrame &frame) {
        if (frame.image.empty() || frame.sequence == 0) {
            invalidFrame = true;
        }
        ++frames;
    };

    QVERIFY(worker.start(CaptureMode::SoftwareTrigger, 0, callbacks));
    QTRY_VERIFY_WITH_TIMEOUT(frames.load() >= 3, 1000);
    worker.stop();

    QVERIFY(!worker.isRunning());
    QVERIFY(camera->softwareTriggerCount.load() >= 3);
    QVERIFY(!invalidFrame.load());
}

void CameraSessionTest::hardwareTriggerWaitsWithoutSoftwareCommand()
{
    const std::shared_ptr<CaptureFakeCamera> camera(
                new CaptureFakeCamera);
    CaptureWorker worker(camera);
    std::atomic<int> frames{0};
    CaptureWorkerCallbacks callbacks;
    callbacks.frameReady = [&frames](const CameraFrame &) {
        ++frames;
    };

    QVERIFY(worker.start(CaptureMode::HardwareTrigger, 500, callbacks));
    QTRY_VERIFY_WITH_TIMEOUT(camera->waitCount.load() > 0, 500);
    QCOMPARE(camera->softwareTriggerCount.load(), 0);
    camera->enqueueFrame(7);
    camera->enqueueFrame(8);
    QTRY_COMPARE_WITH_TIMEOUT(frames.load(), 2, 400);
    worker.stop();
    QCOMPARE(camera->softwareTriggerCount.load(), 0);
}

void CameraSessionTest::previewFramesRemainOutsideProductAdmission()
{
    const std::shared_ptr<CaptureFakeCamera> camera(
                new CaptureFakeCamera);
    camera->autoFrameOnSoftware = true;
    CaptureWorker worker(camera);
    std::atomic<std::uint64_t> latestSequence{0};
    CaptureWorkerCallbacks callbacks;
    callbacks.frameReady = [&latestSequence](const CameraFrame &frame) {
        latestSequence.store(frame.sequence);
    };

    QVERIFY(worker.start(CaptureMode::Preview, 0, callbacks));
    QTRY_VERIFY_WITH_TIMEOUT(latestSequence.load() > 0, 500);
    worker.stop();

    // CameraFrame intentionally carries only camera sequence/time/image.
    // ProductKey is created exclusively when a formal frame enters Runtime.
    QVERIFY(latestSequence.load() > 0);
}

void CameraSessionTest::zeroAndNonZeroIntervalsAreHonored()
{
    const std::shared_ptr<CaptureFakeCamera> camera(
                new CaptureFakeCamera);
    camera->autoFrameOnSoftware = true;
    CaptureWorker worker(camera);
    std::mutex timesMutex;
    std::vector<std::chrono::steady_clock::time_point> times;
    std::atomic<int> frameCount{0};
    CaptureWorkerCallbacks callbacks;
    callbacks.frameReady = [
            &timesMutex, &times, &frameCount](const CameraFrame &) {
        std::lock_guard<std::mutex> lock(timesMutex);
        times.push_back(std::chrono::steady_clock::now());
        ++frameCount;
    };

    QVERIFY(worker.start(CaptureMode::SoftwareTrigger, 40, callbacks));
    QTRY_VERIFY_WITH_TIMEOUT(frameCount.load() >= 3, 1000);
    worker.stop();

    std::lock_guard<std::mutex> lock(timesMutex);
    QVERIFY(times.size() >= 3U);
    for (std::size_t i = 1; i < times.size(); ++i) {
        const qint64 elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    times[i] - times[i - 1]).count();
        QVERIFY2(elapsed >= 35, "non-zero cameraDelay was not honored");
    }
}

void CameraSessionTest::stopInterruptsWaitAndJoinsBeforeReturning()
{
    const std::shared_ptr<CaptureFakeCamera> camera(
                new CaptureFakeCamera);
    CaptureWorker worker(camera);
    std::atomic<int> stopped{0};
    CaptureWorkerCallbacks callbacks;
    callbacks.stopped = [&stopped]() { ++stopped; };

    QVERIFY(worker.start(CaptureMode::HardwareTrigger, 0, callbacks));
    QTRY_VERIFY_WITH_TIMEOUT(camera->waitCount.load() > 0, 500);
    worker.stop();

    QVERIFY(camera->interruptCount.load() > 0);
    QVERIFY(!worker.isRunning());
    QCOMPARE(stopped.load(), 1);

    camera->setTriggerMode(CameraTriggerMode::HardwareLine0);
    QVERIFY(worker.start(CaptureMode::HardwareTrigger, 0, callbacks));
    worker.stop();
    QCOMPARE(stopped.load(), 2);
}

void CameraSessionTest::preprocessorOwnsRotationAndChannelSelection()
{
    cv::Mat source(2, 3, CV_8UC3);
    for (int y = 0; y < source.rows; ++y) {
        for (int x = 0; x < source.cols; ++x) {
            source.at<cv::Vec3b>(y, x) = cv::Vec3b(
                        static_cast<uchar>(x),
                        static_cast<uchar>(y),
                        static_cast<uchar>(10 + y * 3 + x));
        }
    }
    FramePreprocessSettings settings;
    settings.rotation = FrameRotation::Clockwise90;
    settings.colorChannel = FrameColorChannel::Red;
    cv::Mat output;

    QVERIFY(FramePreprocessor::transform(source, settings, &output));
    QCOMPARE(output.rows, 3);
    QCOMPARE(output.cols, 2);
    QCOMPARE(output.channels(), 1);
    QCOMPARE(output.at<uchar>(0, 1), source.at<cv::Vec3b>(0, 0)[2]);
}

void CameraSessionTest::positionerOwnsSingleAndProfileTracking()
{
    const cv::Mat tracking = patternedTemplate();
    cv::Mat frame(100, 120, CV_8UC1, cv::Scalar(0));
    tracking.copyTo(frame(cv::Rect(35, 45, tracking.cols, tracking.rows)));
    const std::vector<cv::Point2f> datePolygon = {
        cv::Point2f(-5.0f, -3.0f),
        cv::Point2f(5.0f, -3.0f),
        cv::Point2f(5.0f, 3.0f),
        cv::Point2f(-5.0f, 3.0f)
    };

    InspectionPositioner single;
    QVERIFY(single.configure(
                InspectionTrackingKind::SingleTemplate,
                std::vector<WordTrackingProfile>(),
                datePolygon,
                tracking));
    const DetectionPose singlePose = single.locate(frame);
    QVERIFY(singlePose.valid);
    QVERIFY(std::abs(singlePose.anchorCenter.x - 45.0f) <= 2.0f);
    QVERIFY(std::abs(singlePose.anchorCenter.y - 55.0f) <= 2.0f);

    WordTrackingProfile profile;
    profile.name = QStringLiteral("P1");
    profile.profileIndex = 7;
    profile.trackingTemplate = tracking;
    profile.datePoly = datePolygon;
    profile.barcodePoly = datePolygon;
    InspectionPositioner profiles;
    QVERIFY(profiles.configure(
                InspectionTrackingKind::WordProfiles,
                std::vector<WordTrackingProfile>(1, profile),
                std::vector<cv::Point2f>(),
                cv::Mat()));
    const DetectionPose profilePose = profiles.locate(frame);
    QVERIFY(profilePose.valid);
    QCOMPARE(profilePose.wordTemplateProfileIndex, 7);
    QCOMPARE(profilePose.barcodePoly.size(), std::size_t(4));
}

QTEST_GUILESS_MAIN(CameraSessionTest)

#include "camera_session_test.moc"
