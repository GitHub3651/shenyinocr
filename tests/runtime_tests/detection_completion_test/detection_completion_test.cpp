#include <QtTest>

#include "TrackingTypes.h"
#include "runtime/detection_session.h"
#include "runtime/image_save_service.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>

namespace {
ProductKey testProductKey(quint64 sequence)
{
    ProductKey key;
    key.runId = QStringLiteral("test-run");
    key.sequence = sequence;
    return key;
}

ImageSaveTask testSaveTask(
    quint64 sequence,
    const QStringList &itemNames)
{
    ImageSaveTask task;
    task.productKey = testProductKey(sequence);
    for (const QString &itemName : itemNames) {
        ImageSaveItem item;
        item.image = QImage(2, 2, QImage::Format_RGB32);
        item.image.fill(Qt::white);
        item.filePath = itemName;
        item.format = QByteArrayLiteral("PNG");
        task.items.push_back(item);
    }
    return task;
}
}

class DetectionCompletionTest : public QObject
{
    Q_OBJECT

private slots:
    void productKeyRequiresRunAndPositiveSequence();
    void frameFactoryOwnsIndependentImage();
    void completionRequiresFrameAndValidProductKey();
    void resultCarriesVerdictStatusTextAndTimingWithoutImage();
    void overlayPreservesOrderedBusinessPolygons();
    void immutableFrameCanBeSharedForShortLivedConsumers();
    void sessionBeginCreatesNewRunAndResetsProductSequence();
    void sessionCompletionOwnsFrameAndCopiesDetectionResult();
    void saveTaskRequiresProductAndAllItems();
    void saveServicePreservesTaskAndItemOrder();
    void fullQueueWaitsForSpaceWithoutDroppingTask();
    void writeFailureIsCountedAndReported();
    void shutdownRejectsNewTasks();
};

void DetectionCompletionTest::productKeyRequiresRunAndPositiveSequence()
{
    ProductKey key;
    QVERIFY(!key.isValid());

    key.runId = QStringLiteral("run-1");
    QVERIFY(!key.isValid());

    key.sequence = 1;
    QVERIFY(key.isValid());
}

void DetectionCompletionTest::frameFactoryOwnsIndependentImage()
{
    ProductKey key;
    key.runId = QStringLiteral("run-1");
    key.sequence = 7;

    cv::Mat source(2, 2, CV_8UC1, cv::Scalar(31));
    const QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(
                123456,
                Qt::UTC);
    const std::shared_ptr<const FrameData> frame = makeFrameData(
                key,
                19,
                2,
                timestamp,
                source);

    source.setTo(cv::Scalar(99));

    QVERIFY(frame);
    QCOMPARE(frame->productKey.runId, QStringLiteral("run-1"));
    QCOMPARE(frame->productKey.sequence, quint64(7));
    QCOMPARE(frame->frameNumber, quint64(19));
    QCOMPARE(frame->cameraIndex, 2);
    QCOMPARE(frame->timestampUtc, timestamp);
    QCOMPARE(frame->originalImage.at<uchar>(0, 0), uchar(31));
}

void DetectionCompletionTest::completionRequiresFrameAndValidProductKey()
{
    DetectionCompletion completion;
    QVERIFY(!completion.isValid());

    ProductKey invalidKey;
    invalidKey.runId = QStringLiteral("run-1");
    completion.frame = makeFrameData(
                invalidKey,
                1,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)));
    QVERIFY(!completion.isValid());

    ProductKey validKey;
    validKey.runId = QStringLiteral("run-1");
    validKey.sequence = 1;
    completion.frame = makeFrameData(
                validKey,
                1,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)));
    QVERIFY(completion.isValid());
}

void DetectionCompletionTest::resultCarriesVerdictStatusTextAndTimingWithoutImage()
{
    DetectionResult result;
    result.modeId = QStringLiteral("ocr");
    result.verdict = AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = QStringLiteral("2026-08-13");
    result.diagnostic = QStringLiteral("target mismatch");
    result.elapsedMs = 12.5;

    QCOMPARE(result.modeId, QStringLiteral("ocr"));
    QVERIFY(result.verdict == AlgorithmVerdict::Ng);
    QVERIFY(result.status == DetectionStatus::Completed);
    QCOMPARE(result.recognizedText, QStringLiteral("2026-08-13"));
    QCOMPARE(result.diagnostic, QStringLiteral("target mismatch"));
    QCOMPARE(result.elapsedMs, 12.5);
}

void DetectionCompletionTest::overlayPreservesOrderedBusinessPolygons()
{
    DetectionResult result;
    DetectionOverlayPolygon tracking;
    tracking.role = QStringLiteral("tracking");
    tracking.points.push_back(cv::Point(1, 2));
    tracking.score = 0.8;
    result.overlay.polygons.push_back(tracking);

    DetectionOverlayPolygon date;
    date.role = QStringLiteral("date");
    date.points.push_back(cv::Point(3, 4));
    result.overlay.polygons.push_back(date);

    QCOMPARE(static_cast<int>(result.overlay.polygons.size()), 2);
    QCOMPARE(result.overlay.polygons[0].role, QStringLiteral("tracking"));
    QCOMPARE(result.overlay.polygons[0].score, 0.8);
    QCOMPARE(result.overlay.polygons[1].role, QStringLiteral("date"));
    QCOMPARE(result.overlay.polygons[1].points[0].x, 3);
}

void DetectionCompletionTest::immutableFrameCanBeSharedForShortLivedConsumers()
{
    ProductKey key;
    key.runId = QStringLiteral("run-2");
    key.sequence = 3;

    DetectionCompletion completion;
    completion.frame = makeFrameData(
                key,
                3,
                0,
                QDateTime::currentDateTimeUtc(),
                cv::Mat(3, 4, CV_8UC3, cv::Scalar(4, 5, 6)));
    const std::shared_ptr<const FrameData> saveConsumer = completion.frame;

    QCOMPARE(completion.frame.get(), saveConsumer.get());
    QCOMPARE(saveConsumer->originalImage.cols, 4);
    QCOMPARE(saveConsumer->originalImage.rows, 3);
    QCOMPARE(saveConsumer.use_count(), 2L);
}

void DetectionCompletionTest::sessionBeginCreatesNewRunAndResetsProductSequence()
{
    int runNumber = 0;
    DetectionSession session([&runNumber]() {
        return QStringLiteral("run-%1").arg(++runNumber);
    });
    QVERIFY(!session.isActive());

    QCOMPARE(session.begin(), QStringLiteral("run-1"));
    QCOMPARE(session.completedProductCount(), quint64(0));

    DetectionResult result;
    result.status = DetectionStatus::Completed;
    const DetectionCompletion first = session.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)),
                result);
    const DetectionCompletion second = session.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(2)),
                result);
    QCOMPARE(first.frame->productKey.runId, QStringLiteral("run-1"));
    QCOMPARE(first.frame->productKey.sequence, quint64(1));
    QCOMPARE(second.frame->productKey.sequence, quint64(2));
    QCOMPARE(session.completedProductCount(), quint64(2));

    QCOMPARE(session.begin(), QStringLiteral("run-2"));
    QCOMPARE(session.completedProductCount(), quint64(0));
    const DetectionCompletion restarted = session.complete(
                cv::Mat(1, 1, CV_8UC1, cv::Scalar(3)),
                result);
    QCOMPARE(restarted.frame->productKey.runId, QStringLiteral("run-2"));
    QCOMPARE(restarted.frame->productKey.sequence, quint64(1));
}

void DetectionCompletionTest::sessionCompletionOwnsFrameAndCopiesDetectionResult()
{
    DetectionSession session([]() {
        return QStringLiteral("fixed-run");
    });
    cv::Mat source(2, 3, CV_8UC1, cv::Scalar(17));
    const QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(
                987654,
                Qt::UTC);

    DetectionResult result;
    result.modeId = QStringLiteral("word");
    result.verdict = AlgorithmVerdict::Ng;
    result.status = DetectionStatus::Completed;
    result.recognizedText = QStringLiteral("123");
    result.diagnostic = QStringLiteral("count mismatch");
    result.elapsedMs = 4.5;
    DetectionOverlayPolygon polygon;
    polygon.role = QStringLiteral("character");
    polygon.points.push_back(cv::Point(2, 3));
    result.overlay.polygons.push_back(polygon);

    const DetectionCompletion completion = session.complete(
                source,
                result,
                41,
                2,
                timestamp);
    source.setTo(cv::Scalar(99));
    result.recognizedText = QStringLiteral("changed");

    QVERIFY(completion.isValid());
    QCOMPARE(completion.frame->productKey.runId, QStringLiteral("fixed-run"));
    QCOMPARE(completion.frame->productKey.sequence, quint64(1));
    QCOMPARE(completion.frame->frameNumber, quint64(41));
    QCOMPARE(completion.frame->cameraIndex, 2);
    QCOMPARE(completion.frame->timestampUtc, timestamp);
    QCOMPARE(completion.frame->originalImage.at<uchar>(0, 0), uchar(17));
    QCOMPARE(completion.result.recognizedText, QStringLiteral("123"));
    QCOMPARE(completion.result.overlay.polygons[0].role,
             QStringLiteral("character"));
}

void DetectionCompletionTest::saveTaskRequiresProductAndAllItems()
{
    ImageSaveTask task;
    QVERIFY(!task.isValid());

    task.productKey = testProductKey(1);
    QVERIFY(!task.isValid());

    ImageSaveItem invalidItem;
    invalidItem.filePath = QStringLiteral("invalid.png");
    invalidItem.format = QByteArrayLiteral("PNG");
    task.items.push_back(invalidItem);
    QVERIFY(!task.isValid());

    task.items[0].image = QImage(1, 1, QImage::Format_RGB32);
    QVERIFY(task.isValid());
}

void DetectionCompletionTest::saveServicePreservesTaskAndItemOrder()
{
    QStringList writtenPaths;
    std::mutex pathsMutex;
    ImageSaveService service(
                8,
                [&writtenPaths, &pathsMutex](
                    const ImageSaveItem &item,
                    QString *) {
        std::lock_guard<std::mutex> lock(pathsMutex);
        writtenPaths.append(item.filePath);
        return true;
    },
    1);

    QVERIFY(service.submit(testSaveTask(
                               1,
                               QStringList()
                               << QStringLiteral("a-annotated.png")
                               << QStringLiteral("a-raw.png")))
            .isAccepted());
    QVERIFY(service.submit(testSaveTask(
                               2,
                               QStringList()
                               << QStringLiteral("b-annotated.png")))
            .isAccepted());

    QTRY_VERIFY(service.outstandingTaskCount() == 0);
    {
        std::lock_guard<std::mutex> lock(pathsMutex);
        QCOMPARE(
                    writtenPaths,
                    QStringList()
                    << QStringLiteral("a-annotated.png")
                    << QStringLiteral("a-raw.png")
                    << QStringLiteral("b-annotated.png"));
    }
}

void DetectionCompletionTest::fullQueueWaitsForSpaceWithoutDroppingTask()
{
    std::mutex gateMutex;
    std::condition_variable gateCondition;
    bool firstWriteStarted = false;
    bool releaseFirstWrite = false;
    int writeCount = 0;
    ImageSaveService service(
                2,
                [&](const ImageSaveItem &, QString *) {
        std::unique_lock<std::mutex> lock(gateMutex);
        ++writeCount;
        if (writeCount == 1) {
            firstWriteStarted = true;
            gateCondition.notify_all();
            gateCondition.wait(lock, [&releaseFirstWrite]() {
                return releaseFirstWrite;
            });
        }
        return true;
    },
    1);

    const bool firstAccepted = service.submit(testSaveTask(
                                                   1,
                                                   QStringList()
                                                   << QStringLiteral("1.png")))
            .isAccepted();
    bool firstStarted = false;
    {
        std::unique_lock<std::mutex> lock(gateMutex);
        firstStarted = gateCondition.wait_for(
                    lock,
                    std::chrono::seconds(2),
                    [&firstWriteStarted]() {
            return firstWriteStarted;
        });
    }

    const bool secondAccepted = service.submit(testSaveTask(
                                                   2,
                                                   QStringList()
                                                   << QStringLiteral("2.png")))
            .isAccepted();
    std::future<ImageSaveSubmitResult> thirdSubmission = std::async(
                std::launch::async,
                [&service]() {
        return service.submit(testSaveTask(
                                  3,
                                  QStringList()
                                  << QStringLiteral("3.png")));
    });
    const bool thirdWaitedForSpace = thirdSubmission.wait_for(
                std::chrono::milliseconds(100))
            == std::future_status::timeout;

    {
        std::lock_guard<std::mutex> lock(gateMutex);
        releaseFirstWrite = true;
    }
    gateCondition.notify_all();
    const bool thirdCompleted = thirdSubmission.wait_for(
                std::chrono::seconds(2))
            == std::future_status::ready;
    ImageSaveSubmitResult thirdResult;
    if (thirdCompleted) {
        thirdResult = thirdSubmission.get();
    }
    QTRY_VERIFY(service.outstandingTaskCount() == 0);
    int finalWriteCount = 0;
    {
        std::lock_guard<std::mutex> lock(gateMutex);
        finalWriteCount = writeCount;
    }

    QVERIFY(firstAccepted);
    QVERIFY(firstStarted);
    QVERIFY(secondAccepted);
    QVERIFY(thirdWaitedForSpace);
    QVERIFY(thirdCompleted);
    QVERIFY(thirdResult.isAccepted());
    QCOMPARE(static_cast<int>(service.capacity()), 2);
    QCOMPARE(static_cast<int>(service.workerCount()), 1);
    QCOMPARE(finalWriteCount, 3);
}

void DetectionCompletionTest::writeFailureIsCountedAndReported()
{
    ImageSaveService service(
                8,
                [](const ImageSaveItem &, QString *errorMessage) {
        *errorMessage = QStringLiteral("simulated disk failure");
        return false;
    },
    1);
    QSignalSpy failureSpy(&service, &ImageSaveService::taskFailed);

    QVERIFY(service.submit(testSaveTask(
                               1,
                               QStringList() << QStringLiteral("failure.png")))
            .isAccepted());
    QTRY_COMPARE(failureSpy.count(), 1);
    QCOMPARE(service.failedTaskCount(), quint64(1));
    QCOMPARE(
                failureSpy.at(0).at(1).toString(),
                QStringLiteral("simulated disk failure"));
}

void DetectionCompletionTest::shutdownRejectsNewTasks()
{
    ImageSaveService service(
                8,
                [](const ImageSaveItem &, QString *) {
        return true;
    },
    1);
    service.shutdown();

    const ImageSaveSubmitResult result = service.submit(
                testSaveTask(
                    1,
                    QStringList() << QStringLiteral("after-stop.png")));
    QVERIFY(result.status == ImageSaveSubmitStatus::Stopping);
}

QTEST_APPLESS_MAIN(DetectionCompletionTest)

#include "detection_completion_test.moc"
