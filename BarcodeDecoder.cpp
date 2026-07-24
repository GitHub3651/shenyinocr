#include "BarcodeDecoder.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QtGlobal>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <limits>

namespace {

const char DecoderLibraryFileName[] = "BarcodeDecoder.dll";
const int DecoderTextCapacity = 8192;

double elapsedMilliseconds(const QElapsedTimer &timer)
{
    return static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
}

QString defaultDecoderLibraryFilePath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QString::fromLatin1(DecoderLibraryFileName));
}

BarcodeReadResult invalidRoiResult(const QString &reason)
{
    BarcodeReadResult result;
    result.status = BarcodeReadStatus::InvalidRoi;
    result.errorReason = reason;
    return result;
}

} // namespace

BarcodeDecoder::BarcodeDecoder(const QString &libraryFilePath)
    : m_getVersion(nullptr)
    , m_decodeLuma8(nullptr)
{
    load(libraryFilePath);
}

BarcodeDecoder::~BarcodeDecoder()
{
    unload();
}

bool BarcodeDecoder::load(const QString &libraryFilePath)
{
    unload();

    m_libraryFilePath = libraryFilePath.trimmed();
    if (m_libraryFilePath.isEmpty()) {
        m_libraryFilePath = defaultDecoderLibraryFilePath();
    } else {
        m_libraryFilePath = QDir::cleanPath(m_libraryFilePath);
    }

    m_library.setFileName(m_libraryFilePath);
    if (!m_library.load()) {
        m_lastError = QStringLiteral("Failed to load barcode decoder DLL: %1; %2")
            .arg(m_libraryFilePath, m_library.errorString());
        return false;
    }

    m_getVersion = reinterpret_cast<BarcodeDecoderGetVersionFunction>(
        m_library.resolve("BarcodeDecoder_GetVersion"));
    m_decodeLuma8 = reinterpret_cast<BarcodeDecoderDecodeLuma8Function>(
        m_library.resolve("BarcodeDecoder_DecodeLuma8"));

    if (!m_getVersion || !m_decodeLuma8) {
        m_lastError = QStringLiteral("Barcode decoder DLL is missing required exports: %1")
            .arg(m_library.errorString());
        unload();
        return false;
    }

    QByteArray versionBuffer(256, '\0');
    const int versionResult = m_getVersion(
        versionBuffer.data(),
        versionBuffer.size());
    if (versionResult > 0) {
        int versionLength = versionBuffer.indexOf('\0');
        if (versionLength < 0) {
            versionLength = versionBuffer.size();
        }
        m_version = QString::fromUtf8(
            versionBuffer.constData(),
            versionLength).trimmed();
    }

    if (m_version.isEmpty()) {
        m_version = QStringLiteral("unknown");
    }

    m_lastError.clear();
    return true;
}

void BarcodeDecoder::unload()
{
    m_getVersion = nullptr;
    m_decodeLuma8 = nullptr;
    m_version.clear();

    if (m_library.isLoaded()) {
        m_library.unload();
    }
}

bool BarcodeDecoder::isAvailable() const
{
    return m_library.isLoaded() && m_getVersion && m_decodeLuma8;
}

QString BarcodeDecoder::version() const
{
    return m_version;
}

QString BarcodeDecoder::libraryFilePath() const
{
    return m_libraryFilePath;
}

QString BarcodeDecoder::lastError() const
{
    return m_lastError;
}

BarcodeReadResult BarcodeDecoder::decode(
    const cv::Mat &image,
    const BarcodeDecodeOptions &options)
{
    if (!isAvailable() && !load(m_libraryFilePath)) {
        BarcodeReadResult result;
        result.status = BarcodeReadStatus::DecoderUnavailable;
        result.errorReason = m_lastError;
        return result;
    }

    if (image.empty() || image.cols <= 0 || image.rows <= 0) {
        return invalidRoiResult(QStringLiteral("Barcode ROI is empty"));
    }

    cv::Mat grayImage;
    try {
        if (image.channels() == 1) {
            if (image.depth() == CV_8U) {
                grayImage = image;
            } else {
                image.convertTo(grayImage, CV_8U);
            }
        } else if (image.channels() == 3) {
            cv::cvtColor(image, grayImage, cv::COLOR_BGR2GRAY);
        } else if (image.channels() == 4) {
            cv::cvtColor(image, grayImage, cv::COLOR_BGRA2GRAY);
        } else {
            return invalidRoiResult(
                QStringLiteral("Unsupported barcode ROI channel count: %1")
                .arg(image.channels()));
        }

        if (!grayImage.isContinuous()) {
            grayImage = grayImage.clone();
        }
    } catch (const cv::Exception &exception) {
        BarcodeReadResult result;
        result.status = BarcodeReadStatus::InternalError;
        result.errorReason = QStringLiteral("Failed to convert barcode ROI to grayscale: %1")
            .arg(QString::fromLocal8Bit(exception.what()));
        return result;
    }

    const int maxDecodeTimeMs = std::max(1, options.maxDecodeTimeMs);
    QElapsedTimer totalTimer;
    totalTimer.start();

    BarcodeReadResult lastResult = decodeOnce(
        grayImage,
        options.formatMask,
        BARCODE_DECODER_OPTION_NONE);
    lastResult.elapsedMs = elapsedMilliseconds(totalTimer);

    if (lastResult.readable || isFatalDecodeStatus(lastResult.status)) {
        return lastResult;
    }

    if (!options.enableFallback) {
        return lastResult;
    }

    const auto budgetAvailable = [&totalTimer, maxDecodeTimeMs]() {
        return elapsedMilliseconds(totalTimer) < maxDecodeTimeMs;
    };

    const auto runAttempt = [this, &lastResult, &totalTimer, &options](
            const cv::Mat &candidate,
            unsigned int optionFlags) {
        lastResult = decodeOnce(
            candidate,
            options.formatMask,
            optionFlags);
        lastResult.elapsedMs = elapsedMilliseconds(totalTimer);
        return lastResult.readable || isFatalDecodeStatus(lastResult.status);
    };

    try {
        if (budgetAvailable()) {
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
            cv::Mat enhanced;
            clahe->apply(grayImage, enhanced);
            if (runAttempt(enhanced, BARCODE_DECODER_OPTION_NONE)) {
                return lastResult;
            }
        }

        if (budgetAvailable()) {
            cv::Mat enlarged;
            cv::resize(
                grayImage,
                enlarged,
                cv::Size(),
                2.0,
                2.0,
                cv::INTER_CUBIC);
            if (runAttempt(enlarged, BARCODE_DECODER_OPTION_NONE)) {
                return lastResult;
            }
        }

        cv::Mat binaryImage;
        if (budgetAvailable()) {
            cv::threshold(
                grayImage,
                binaryImage,
                0,
                255,
                cv::THRESH_BINARY | cv::THRESH_OTSU);
            if (runAttempt(binaryImage, BARCODE_DECODER_OPTION_NONE)) {
                return lastResult;
            }
        }

        if (budgetAvailable() && !binaryImage.empty()) {
            cv::Mat invertedImage;
            cv::bitwise_not(binaryImage, invertedImage);
            if (runAttempt(invertedImage, BARCODE_DECODER_OPTION_NONE)) {
                return lastResult;
            }
        }

        if (budgetAvailable()) {
            if (runAttempt(
                    grayImage,
                    BARCODE_DECODER_OPTION_TRY_HARDER)) {
                return lastResult;
            }
        }
    } catch (const cv::Exception &exception) {
        lastResult.status = BarcodeReadStatus::InternalError;
        lastResult.readable = false;
        lastResult.elapsedMs = elapsedMilliseconds(totalTimer);
        lastResult.errorReason =
            QStringLiteral("Barcode fallback preprocessing failed: %1")
            .arg(QString::fromLocal8Bit(exception.what()));
        return lastResult;
    }

    lastResult.elapsedMs = elapsedMilliseconds(totalTimer);
    if (lastResult.elapsedMs >= maxDecodeTimeMs) {
        lastResult.status = BarcodeReadStatus::Timeout;
        lastResult.errorReason = QStringLiteral("Barcode decoding exceeded the %1 ms budget")
            .arg(maxDecodeTimeMs);
    }

    return lastResult;
}

BarcodeReadResult BarcodeDecoder::decodeOnce(
    const cv::Mat &grayImage,
    unsigned int formatMask,
    unsigned int optionFlags) const
{
    if (!isAvailable()) {
        BarcodeReadResult result;
        result.status = BarcodeReadStatus::DecoderUnavailable;
        result.errorReason = QStringLiteral("Barcode decoder DLL is unavailable");
        return result;
    }

    if (grayImage.empty()
            || grayImage.type() != CV_8UC1
            || grayImage.cols <= 0
            || grayImage.rows <= 0
            || grayImage.step <= 0
            || grayImage.step > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return invalidRoiResult(QStringLiteral("Invalid grayscale barcode ROI"));
    }

    QByteArray textBuffer(DecoderTextCapacity, '\0');
    int textLength = 0;
    int decodedFormat = 0;
    float corners[8] = {0.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f};
    int elapsedMicroseconds = 0;

    const int returnCode = m_decodeLuma8(
        grayImage.ptr<unsigned char>(0),
        grayImage.cols,
        grayImage.rows,
        static_cast<int>(grayImage.step),
        formatMask,
        optionFlags,
        textBuffer.data(),
        textBuffer.size(),
        &textLength,
        &decodedFormat,
        corners,
        &elapsedMicroseconds);

    BarcodeReadResult result;
    if (elapsedMicroseconds > 0) {
        result.elapsedMs =
                static_cast<double>(elapsedMicroseconds) / 1000.0;
    }

    if (returnCode == BARCODE_DECODER_RESULT_SUCCESS) {
        const int safeTextLength = std::max(
            0,
            std::min(textLength, textBuffer.size()));
        result.rawBytes = textBuffer.left(safeTextLength);
        result.text = QString::fromUtf8(
            result.rawBytes.constData(),
            result.rawBytes.size());
        result.format = toBarcodeFormat(decodedFormat);

        for (int index = 0; index < 4; ++index) {
            result.cornersInRoi.emplace_back(
                corners[index * 2],
                corners[index * 2 + 1]);
        }

        result.readable = !result.rawBytes.isEmpty();
        if (result.readable) {
            result.status = BarcodeReadStatus::Success;
        } else {
            result.status = BarcodeReadStatus::InternalError;
            result.errorReason =
                QStringLiteral("Decoder returned success with empty barcode data");
        }
        return result;
    }

    if (returnCode == BARCODE_DECODER_RESULT_NOT_FOUND) {
        result.status = BarcodeReadStatus::NotFound;
        result.errorReason = QStringLiteral("Barcode was not found or is unreadable");
    } else if (returnCode == BARCODE_DECODER_ERROR_INVALID_ARGUMENT) {
        result.status = BarcodeReadStatus::InvalidRoi;
        result.errorReason = QStringLiteral("Decoder rejected the barcode ROI arguments");
    } else if (returnCode == BARCODE_DECODER_ERROR_UNSUPPORTED_FORMAT) {
        result.status = BarcodeReadStatus::InternalError;
        result.errorReason = QStringLiteral("Decoder does not support the requested format");
    } else {
        result.status = BarcodeReadStatus::InternalError;
        result.errorReason = QStringLiteral("Barcode decoder internal error, return code=%1")
            .arg(returnCode);
    }

    return result;
}

bool BarcodeDecoder::isFatalDecodeStatus(BarcodeReadStatus status)
{
    return status == BarcodeReadStatus::InvalidRoi
        || status == BarcodeReadStatus::DecoderUnavailable
        || status == BarcodeReadStatus::InternalError;
}

BarcodeFormat BarcodeDecoder::toBarcodeFormat(int decoderFormat)
{
    if (decoderFormat == static_cast<int>(BARCODE_DECODER_FORMAT_DATA_MATRIX)) {
        return BarcodeFormat::DataMatrix;
    }
    if (decoderFormat == static_cast<int>(BARCODE_DECODER_FORMAT_QR_CODE)) {
        return BarcodeFormat::QRCode;
    }
    return BarcodeFormat::Unknown;
}
