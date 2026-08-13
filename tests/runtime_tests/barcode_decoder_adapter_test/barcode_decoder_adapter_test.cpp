#include <QtTest>

#include "devices/barcode/barcode_decoder_adapter.h"

#include <cstring>
#include <vector>

namespace {

enum class FakeDecodeMode
{
    Success,
    NotFound,
    InvalidArgument,
    SuccessWithTryHarder
};

FakeDecodeMode g_mode = FakeDecodeMode::Success;
int g_decodeCalls = 0;
std::vector<unsigned int> g_optionFlags;

int BARCODE_DECODER_CALL fakeGetVersion(
    char *utf8Buffer,
    int bufferCapacity)
{
    const QByteArray version("fake-1.0");
    if (!utf8Buffer || bufferCapacity <= version.size()) {
        return BARCODE_DECODER_ERROR_INVALID_ARGUMENT;
    }
    std::memcpy(
                utf8Buffer,
                version.constData(),
                static_cast<size_t>(version.size()));
    utf8Buffer[version.size()] = '\0';
    return version.size();
}

int BARCODE_DECODER_CALL fakeDecodeLuma8(
    const unsigned char *,
    int,
    int,
    int,
    unsigned int,
    unsigned int optionFlags,
    char *utf8Text,
    int textCapacity,
    int *textLength,
    int *decodedFormat,
    float corners[8],
    int *elapsedMicroseconds)
{
    ++g_decodeCalls;
    g_optionFlags.push_back(optionFlags);

    if (g_mode == FakeDecodeMode::InvalidArgument) {
        return BARCODE_DECODER_ERROR_INVALID_ARGUMENT;
    }
    if (g_mode == FakeDecodeMode::NotFound
            || (g_mode == FakeDecodeMode::SuccessWithTryHarder
                && (optionFlags & BARCODE_DECODER_OPTION_TRY_HARDER) == 0)) {
        return BARCODE_DECODER_RESULT_NOT_FOUND;
    }

    const QByteArray text("FAKE-123");
    if (!utf8Text
            || textCapacity < text.size()
            || !textLength
            || !decodedFormat
            || !corners
            || !elapsedMicroseconds) {
        return BARCODE_DECODER_ERROR_INVALID_ARGUMENT;
    }
    std::memcpy(
                utf8Text,
                text.constData(),
                static_cast<size_t>(text.size()));
    *textLength = text.size();
    *decodedFormat = BARCODE_DECODER_FORMAT_DATA_MATRIX;
    for (int index = 0; index < 8; ++index) {
        corners[index] = static_cast<float>(index + 1);
    }
    *elapsedMicroseconds = 2500;
    return BARCODE_DECODER_RESULT_SUCCESS;
}

BarcodeDecoderFunctions fakeFunctions()
{
    BarcodeDecoderFunctions functions;
    functions.getVersion = &fakeGetVersion;
    functions.decodeLuma8 = &fakeDecodeLuma8;
    return functions;
}

void resetFake(FakeDecodeMode mode)
{
    g_mode = mode;
    g_decodeCalls = 0;
    g_optionFlags.clear();
}

} // namespace

class BarcodeDecoderAdapterTest : public QObject
{
    Q_OBJECT

private slots:
    void injectedFunctionsAreReady();
    void successMapsAbiResult();
    void invalidImageIsRejectedBeforeAbiCall();
    void invalidArgumentIsTerminal();
    void disabledFallbackRunsOnlyFastAttempt();
    void preferredFallbackStrategyRunsFirst();
};

void BarcodeDecoderAdapterTest::injectedFunctionsAreReady()
{
    BarcodeDecoderAdapter adapter(fakeFunctions());

    QVERIFY(adapter.ensureLoaded());
    QVERIFY(adapter.lastError().isEmpty());
}

void BarcodeDecoderAdapterTest::successMapsAbiResult()
{
    resetFake(FakeDecodeMode::Success);
    BarcodeDecoderAdapter adapter(fakeFunctions());
    const cv::Mat image(16, 20, CV_8UC1, cv::Scalar(127));

    int strategyId = -1;
    unsigned int optionFlags = 99;
    const BarcodeReadResult result = adapter.decode(
                image,
                BarcodeDecodeOptions(),
                -1,
                BARCODE_DECODER_OPTION_NONE,
                &strategyId,
                &optionFlags);

    QVERIFY(result.readable);
    QCOMPARE(static_cast<int>(result.status),
             static_cast<int>(BarcodeReadStatus::Success));
    QCOMPARE(static_cast<int>(result.format),
             static_cast<int>(BarcodeFormat::DataMatrix));
    QCOMPARE(result.text, QStringLiteral("FAKE-123"));
    QCOMPARE(result.rawBytes, QByteArray("FAKE-123"));
    QCOMPARE(static_cast<int>(result.cornersInRoi.size()), 4);
    QVERIFY(result.cornersInRoi[0] == cv::Point2f(1.0f, 2.0f));
    QCOMPARE(g_decodeCalls, 1);
    QCOMPARE(strategyId, 0);
    QCOMPARE(optionFlags,
             static_cast<unsigned int>(BARCODE_DECODER_OPTION_NONE));
}

void BarcodeDecoderAdapterTest::invalidImageIsRejectedBeforeAbiCall()
{
    resetFake(FakeDecodeMode::Success);
    BarcodeDecoderAdapter adapter(fakeFunctions());

    const BarcodeReadResult result = adapter.decode(
                cv::Mat(),
                BarcodeDecodeOptions());

    QVERIFY(!result.readable);
    QCOMPARE(static_cast<int>(result.status),
             static_cast<int>(BarcodeReadStatus::InvalidRoi));
    QCOMPARE(g_decodeCalls, 0);
}

void BarcodeDecoderAdapterTest::invalidArgumentIsTerminal()
{
    resetFake(FakeDecodeMode::InvalidArgument);
    BarcodeDecoderAdapter adapter(fakeFunctions());
    const cv::Mat image(16, 20, CV_8UC1, cv::Scalar(127));

    const BarcodeReadResult result = adapter.decode(
                image,
                BarcodeDecodeOptions());

    QVERIFY(!result.readable);
    QCOMPARE(static_cast<int>(result.status),
             static_cast<int>(BarcodeReadStatus::InvalidRoi));
    QCOMPARE(g_decodeCalls, 1);
}

void BarcodeDecoderAdapterTest::disabledFallbackRunsOnlyFastAttempt()
{
    resetFake(FakeDecodeMode::NotFound);
    BarcodeDecoderAdapter adapter(fakeFunctions());
    const cv::Mat image(16, 20, CV_8UC1, cv::Scalar(127));
    BarcodeDecodeOptions options;
    options.enableFallback = false;

    const BarcodeReadResult result = adapter.decode(image, options);

    QVERIFY(!result.readable);
    QCOMPARE(static_cast<int>(result.status),
             static_cast<int>(BarcodeReadStatus::NotFound));
    QCOMPARE(g_decodeCalls, 1);
    QCOMPARE(g_optionFlags.front(),
             static_cast<unsigned int>(BARCODE_DECODER_OPTION_NONE));
}

void BarcodeDecoderAdapterTest::preferredFallbackStrategyRunsFirst()
{
    resetFake(FakeDecodeMode::SuccessWithTryHarder);
    BarcodeDecoderAdapter adapter(fakeFunctions());
    const cv::Mat image(24, 24, CV_8UC1, cv::Scalar(127));

    int strategyId = -1;
    unsigned int optionFlags = BARCODE_DECODER_OPTION_NONE;
    const BarcodeReadResult result = adapter.decode(
                image,
                BarcodeDecodeOptions(),
                7,
                BARCODE_DECODER_OPTION_TRY_INVERT,
                &strategyId,
                &optionFlags);

    QVERIFY(result.readable);
    QCOMPARE(g_decodeCalls, 2);
    QCOMPARE(g_optionFlags[0],
             static_cast<unsigned int>(BARCODE_DECODER_OPTION_NONE));
    QCOMPARE(g_optionFlags[1],
             static_cast<unsigned int>(
                 BARCODE_DECODER_OPTION_TRY_HARDER
                 | BARCODE_DECODER_OPTION_TRY_INVERT));
    QCOMPARE(strategyId, 7);
    QCOMPARE(optionFlags, g_optionFlags[1]);
}

QTEST_APPLESS_MAIN(BarcodeDecoderAdapterTest)

#include "barcode_decoder_adapter_test.moc"
