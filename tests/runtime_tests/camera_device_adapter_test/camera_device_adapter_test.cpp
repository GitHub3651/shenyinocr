#include <QtTest>

#include "devices/camera/hikvision_camera_device.h"

class CameraDeviceAdapterTest : public QObject
{
    Q_OBJECT

private slots:
    void discoveryForwardsCountAndResult();
    void openAndCloseForwardIndexAndResults();
    void acquisitionLifecyclePreservesCallOrder();
    void parameterAccessPreservesKeysAndValues();
    void commandExecutionPreservesKeyAndResult();
    void frameAccessPreservesImagesAndSequence();
    void frameStateControlsAreForwarded();
    void missingBackendFailsSafely();
};

void CameraDeviceAdapterTest::discoveryForwardsCountAndResult()
{
    HikvisionCameraFunctions functions;
    functions.enumerateDevices = [](int *deviceCount) {
        *deviceCount = 3;
        return 17;
    };
    HikvisionCameraDevice device(functions);

    int deviceCount = -1;
    const CameraOperationResult result =
        device.enumerateDevices(&deviceCount);

    QCOMPARE(result.nativeErrorCode, 17);
    QCOMPARE(deviceCount, 3);
}

void CameraDeviceAdapterTest::openAndCloseForwardIndexAndResults()
{
    HikvisionCameraFunctions functions;
    int openedIndex = -1;
    int closeCount = 0;
    functions.openDevice = [&openedIndex](int deviceIndex) {
        openedIndex = deviceIndex;
        return 0;
    };
    functions.close = [&closeCount]() {
        ++closeCount;
        return 23;
    };
    HikvisionCameraDevice device(functions);

    QVERIFY(device.openDevice(0).isSuccess());
    QCOMPARE(openedIndex, 0);
    QCOMPARE(device.close().nativeErrorCode, 23);
    QCOMPARE(closeCount, 1);
}

void CameraDeviceAdapterTest::acquisitionLifecyclePreservesCallOrder()
{
    HikvisionCameraFunctions functions;
    QStringList calls;
    functions.registerImageCallback = [&calls]() {
        calls.append(QStringLiteral("callback"));
        return 0;
    };
    functions.startGrabbing = [&calls]() {
        calls.append(QStringLiteral("start"));
        return 0;
    };
    functions.stopGrabbing = [&calls]() {
        calls.append(QStringLiteral("stop"));
        return 0;
    };
    HikvisionCameraDevice device(functions);

    QVERIFY(device.registerImageCallback().isSuccess());
    QVERIFY(device.startGrabbing().isSuccess());
    QVERIFY(device.stopGrabbing().isSuccess());
    QCOMPARE(calls,
             QStringList()
             << QStringLiteral("callback")
             << QStringLiteral("start")
             << QStringLiteral("stop"));
}

void CameraDeviceAdapterTest::parameterAccessPreservesKeysAndValues()
{
    HikvisionCameraFunctions functions;
    QByteArray enumKey;
    unsigned int enumValue = 0;
    QByteArray floatKey;
    float writtenFloat = 0.0f;
    functions.setEnumValue =
        [&enumKey, &enumValue](const char *key, unsigned int value) {
        enumKey = key;
        enumValue = value;
        return 0;
    };
    functions.setFloatValue =
        [&floatKey, &writtenFloat](const char *key, float value) {
        floatKey = key;
        writtenFloat = value;
        return 0;
    };
    functions.getFloatValue =
        [](const char *key, CameraFloatValue *value) {
        if (QByteArray(key) != QByteArray("ExposureTime")) {
            return 9;
        }
        value->minimumValue = 100.0f;
        value->maximumValue = 900.0f;
        value->currentValue = 800.0f;
        return 0;
    };
    functions.getBoolValue = [](const char *key, bool *value) {
        if (QByteArray(key) != QByteArray("LineStatus")) {
            return 11;
        }
        *value = true;
        return 0;
    };
    HikvisionCameraDevice device(functions);

    QVERIFY(device.setEnumValue("TriggerSource", 7).isSuccess());
    QVERIFY(device.setFloatValue("Gain", 2.0f).isSuccess());
    CameraFloatValue exposure;
    QVERIFY(device.getFloatValue("ExposureTime", &exposure).isSuccess());
    bool lineStatus = false;
    QVERIFY(device.getBoolValue("LineStatus", &lineStatus).isSuccess());

    QCOMPARE(enumKey, QByteArray("TriggerSource"));
    QCOMPARE(enumValue, 7U);
    QCOMPARE(floatKey, QByteArray("Gain"));
    QCOMPARE(writtenFloat, 2.0f);
    QCOMPARE(exposure.minimumValue, 100.0f);
    QCOMPARE(exposure.maximumValue, 900.0f);
    QCOMPARE(exposure.currentValue, 800.0f);
    QVERIFY(lineStatus);
}

void CameraDeviceAdapterTest::commandExecutionPreservesKeyAndResult()
{
    HikvisionCameraFunctions functions;
    QByteArray commandKey;
    functions.executeCommand = [&commandKey](const char *key) {
        commandKey = key;
        return 31;
    };
    HikvisionCameraDevice device(functions);

    const CameraOperationResult result =
        device.executeCommand("TriggerSoftware");

    QCOMPARE(commandKey, QByteArray("TriggerSoftware"));
    QCOMPARE(result.nativeErrorCode, 31);
}

void CameraDeviceAdapterTest::frameAccessPreservesImagesAndSequence()
{
    HikvisionCameraFunctions functions;
    functions.readBuffer = [](cv::Mat &image) {
        image = cv::Mat(2, 3, CV_8UC1, cv::Scalar(4)).clone();
        return 0;
    };
    functions.latestImage = []() {
        return cv::Mat(1, 2, CV_8UC1, cv::Scalar(5)).clone();
    };
    functions.waitForImage = []() {
        return cv::Mat(3, 1, CV_8UC1, cv::Scalar(6)).clone();
    };
    functions.takeImageForMainIfReady = [](cv::Mat &image) {
        image = cv::Mat(2, 2, CV_8UC1, cv::Scalar(7)).clone();
        return true;
    };
    functions.frameSequence = []() -> std::uint64_t {
        return 42;
    };
    HikvisionCameraDevice device(functions);

    cv::Mat buffered;
    QVERIFY(device.readBuffer(buffered).isSuccess());
    QCOMPARE(buffered.rows, 2);
    QCOMPARE(buffered.cols, 3);
    QCOMPARE(static_cast<int>(buffered.at<unsigned char>(0, 0)), 4);

    const cv::Mat latest = device.latestImage();
    QCOMPARE(latest.cols, 2);
    QCOMPARE(static_cast<int>(latest.at<unsigned char>(0, 0)), 5);

    const cv::Mat waited = device.waitForImage();
    QCOMPARE(waited.rows, 3);
    QCOMPARE(static_cast<int>(waited.at<unsigned char>(0, 0)), 6);

    cv::Mat ready;
    QVERIFY(device.takeImageForMainIfReady(ready));
    QCOMPARE(ready.rows, 2);
    QCOMPARE(static_cast<int>(ready.at<unsigned char>(0, 0)), 7);
    QCOMPARE(device.frameSequence(), static_cast<std::uint64_t>(42));
}

void CameraDeviceAdapterTest::frameStateControlsAreForwarded()
{
    HikvisionCameraFunctions functions;
    bool ready = true;
    bool nonBlocking = false;
    int deferCount = 0;
    int stopCount = 0;
    functions.isImageReadyForMain = [&ready]() {
        return ready;
    };
    functions.setNonBlocking = [&nonBlocking](bool enabled) {
        nonBlocking = enabled;
    };
    functions.deferSwitchToBlockingAfterNextFrame = [&deferCount]() {
        ++deferCount;
    };
    functions.requestStop = [&stopCount]() {
        ++stopCount;
    };
    HikvisionCameraDevice device(functions);

    QVERIFY(device.isImageReadyForMain());
    device.setNonBlocking(true);
    device.deferSwitchToBlockingAfterNextFrame();
    device.requestStop();

    QVERIFY(nonBlocking);
    QCOMPARE(deferCount, 1);
    QCOMPARE(stopCount, 1);
}

void CameraDeviceAdapterTest::missingBackendFailsSafely()
{
    HikvisionCameraFunctions functions;
    HikvisionCameraDevice device(functions);
    int deviceCount = 9;
    CameraFloatValue floatValue;
    bool boolValue = false;
    cv::Mat image;

    QVERIFY(!device.enumerateDevices(&deviceCount).isSuccess());
    QVERIFY(!device.openDevice(0).isSuccess());
    QVERIFY(!device.close().isSuccess());
    QVERIFY(!device.registerImageCallback().isSuccess());
    QVERIFY(!device.startGrabbing().isSuccess());
    QVERIFY(!device.stopGrabbing().isSuccess());
    QVERIFY(!device.setEnumValue("x", 1).isSuccess());
    QVERIFY(!device.setFloatValue("x", 1.0f).isSuccess());
    QVERIFY(!device.getFloatValue("x", &floatValue).isSuccess());
    QVERIFY(!device.getBoolValue("x", &boolValue).isSuccess());
    QVERIFY(!device.executeCommand("x").isSuccess());
    QVERIFY(!device.readBuffer(image).isSuccess());
    QVERIFY(device.latestImage().empty());
    QVERIFY(device.waitForImage().empty());
    QVERIFY(!device.takeImageForMainIfReady(image));
    QCOMPARE(device.frameSequence(), static_cast<std::uint64_t>(0));
    QVERIFY(!device.isImageReadyForMain());
    device.setNonBlocking(true);
    device.deferSwitchToBlockingAfterNextFrame();
    device.requestStop();
}

QTEST_APPLESS_MAIN(CameraDeviceAdapterTest)

#include "camera_device_adapter_test.moc"
