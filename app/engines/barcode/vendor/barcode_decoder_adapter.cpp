// 文件作用：本文件用于把二维码供应商接口转换为项目内部统一的二维码解码端口。
// 主要职责：把二维码供应商接口转换为项目内部统一的二维码解码端口。
// 模块位置：引擎层；通过统一端口隔离OCR和二维码供应商实现。
// 协作说明：本文件只通过明确的接口与其他模块协作，不改变既有业务行为。
#include "barcode_decoder_adapter.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>

#include <opencv2/imgproc.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {

// 函数说明：elapsedMilliseconds 函数实现名称所表示的处理步骤。
double elapsedMilliseconds(const QElapsedTimer &timer)
{
    return static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
}

} // namespace

// 组件说明：BarcodeDecoderAdapter 数据结构集中保存该流程需要的一组相关数据。
struct BarcodeDecoderAdapter::Impl
{
    HMODULE module = nullptr;
    bool ownsModule = false;
    BarcodeDecoderFunctions functions;
    QString error;
};

// 函数说明：BarcodeDecoderAdapter 构造函数创建组件并初始化其依赖和初始状态。
BarcodeDecoderAdapter::BarcodeDecoderAdapter()
    : m_impl(new Impl)
{
}

// 函数说明：BarcodeDecoderAdapter 构造函数创建组件并初始化其依赖和初始状态。
BarcodeDecoderAdapter::BarcodeDecoderAdapter(
    const BarcodeDecoderFunctions &functions)
    : m_impl(new Impl)
{
    m_impl->functions = functions;
}

// 函数说明：~BarcodeDecoderAdapter 析构函数按生命周期要求释放组件持有的资源。
BarcodeDecoderAdapter::~BarcodeDecoderAdapter()
{
    if (m_impl->ownsModule && m_impl->module) {
        FreeLibrary(m_impl->module);
        m_impl->module = nullptr;
    }
}

// 函数说明：ensureLoaded 函数实现名称所表示的处理步骤。
bool BarcodeDecoderAdapter::ensureLoaded()
{
    if (m_impl->functions.getVersion
            && m_impl->functions.decodeLuma8) {
        return true;
    }

    m_impl->functions = BarcodeDecoderFunctions();
    if (m_impl->ownsModule && m_impl->module) {
        FreeLibrary(m_impl->module);
    }
    m_impl->module = nullptr;
    m_impl->ownsModule = false;

    const QString decoderPath = QDir::toNativeSeparators(
                QDir(QCoreApplication::applicationDirPath())
                .filePath("BarcodeDecoder.dll"));
    HMODULE module = LoadLibraryW(
                reinterpret_cast<LPCWSTR>(decoderPath.utf16()));
    if (!module) {
        const DWORD loadError = GetLastError();
        m_impl->error =
                QString::fromWCharArray(
                    L"无法加载二维码"
                    L"解码DLL：%1（Windows"
                    L"错误码=%2）")
                .arg(decoderPath)
                .arg(static_cast<qulonglong>(loadError));
        return false;
    }

    BarcodeDecoderFunctions functions;
    functions.getVersion =
            reinterpret_cast<BarcodeDecoderGetVersionFunction>(
                GetProcAddress(module, "BarcodeDecoder_GetVersion"));
    functions.decodeLuma8 =
            reinterpret_cast<BarcodeDecoderDecodeLuma8Function>(
                GetProcAddress(module, "BarcodeDecoder_DecodeLuma8"));
    if (!functions.getVersion || !functions.decodeLuma8) {
        m_impl->error =
                QString::fromWCharArray(
                    L"二维码解码DLL"
                    L"缺少接口：%1")
                .arg(decoderPath);
        FreeLibrary(module);
        return false;
    }

    m_impl->module = module;
    m_impl->ownsModule = true;
    m_impl->functions = functions;
    m_impl->error.clear();

    QByteArray versionBuffer(256, '\0');
    const int versionResult = m_impl->functions.getVersion(
                versionBuffer.data(),
                versionBuffer.size());
    const QString decoderVersion =
            versionResult > 0
            ? QString::fromUtf8(
                versionBuffer.constData(),
                std::min(versionResult, versionBuffer.size()))
            : QStringLiteral("unknown");
    qDebug() << "[BARCODE_WORD] Decoder DLL loaded:"
             << decoderPath
             << "version:"
             << decoderVersion;
    return true;
}

// 函数说明：lastError 函数实现名称所表示的处理步骤。
QString BarcodeDecoderAdapter::lastError() const
{
    return m_impl->error;
}

// 函数说明：decodeOnce 函数校验、转换或恢复对应数据。
BarcodeReadResult BarcodeDecoderAdapter::decodeOnce(
    const cv::Mat &grayRoi,
    unsigned int formatMask,
    unsigned int optionFlags) const
{
    BarcodeReadResult result;
    if (!m_impl->functions.decodeLuma8) {
        result.status = BarcodeReadStatus::DecoderUnavailable;
        result.errorReason = "Barcode decoder function is unavailable";
        return result;
    }

    if (grayRoi.empty()
            || grayRoi.type() != CV_8UC1
            || !grayRoi.isContinuous()
            || grayRoi.cols <= 0
            || grayRoi.rows <= 0
            || grayRoi.step <= 0
            || grayRoi.step > static_cast<size_t>(
                std::numeric_limits<int>::max())) {
        result.status = BarcodeReadStatus::InvalidRoi;
        result.errorReason = "Invalid continuous grayscale barcode ROI";
        return result;
    }

    QByteArray textBuffer(8192, '\0');
    int textLength = 0;
    int decodedFormat = 0;
    float corners[8] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };
    int elapsedMicroseconds = 0;

    const int returnCode = m_impl->functions.decodeLuma8(
                grayRoi.ptr<unsigned char>(0),
                grayRoi.cols,
                grayRoi.rows,
                static_cast<int>(grayRoi.step),
                formatMask,
                optionFlags,
                textBuffer.data(),
                textBuffer.size(),
                &textLength,
                &decodedFormat,
                corners,
                &elapsedMicroseconds);

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

        if (decodedFormat
                == static_cast<int>(
                    BARCODE_DECODER_FORMAT_DATA_MATRIX)) {
            result.format = BarcodeFormat::DataMatrix;
        } else if (decodedFormat
                   == static_cast<int>(
                       BARCODE_DECODER_FORMAT_QR_CODE)) {
            result.format = BarcodeFormat::QRCode;
        }

        for (int index = 0; index < 4; ++index) {
            result.cornersInRoi.emplace_back(
                        corners[index * 2],
                        corners[index * 2 + 1]);
        }

        result.readable = !result.rawBytes.isEmpty();
        result.status = result.readable
                ? BarcodeReadStatus::Success
                : BarcodeReadStatus::InternalError;
        if (!result.readable) {
            result.errorReason =
                    "Decoder returned success with empty barcode data";
        }
        return result;
    }

    if (returnCode == BARCODE_DECODER_RESULT_NOT_FOUND) {
        result.status = BarcodeReadStatus::NotFound;
        result.errorReason = "Barcode was not found or is unreadable";
    } else if (returnCode
               == BARCODE_DECODER_ERROR_INVALID_ARGUMENT) {
        result.status = BarcodeReadStatus::InvalidRoi;
        result.errorReason = "Decoder rejected barcode ROI arguments";
    } else {
        result.status = BarcodeReadStatus::InternalError;
        result.errorReason =
                QString("Barcode decoder error, return code=%1")
                .arg(returnCode);
    }
    return result;
}

// 函数说明：decode 函数校验、转换或恢复对应数据。
BarcodeReadResult BarcodeDecoderAdapter::decode(
    const cv::Mat &grayRoi,
    const BarcodeDecodeOptions &options,
    int preferredStrategyId,
    unsigned int preferredOptionFlags,
    int *successfulStrategyId,
    unsigned int *successfulOptionFlags)
{
    if (successfulStrategyId) {
        *successfulStrategyId = -1;
    }
    if (successfulOptionFlags) {
        *successfulOptionFlags = BARCODE_DECODER_OPTION_NONE;
    }
    if (!ensureLoaded()) {
        BarcodeReadResult result;
        result.status = BarcodeReadStatus::DecoderUnavailable;
        result.errorReason = m_impl->error;
        return result;
    }

    if (grayRoi.empty()
            || grayRoi.type() != CV_8UC1
            || grayRoi.cols <= 0
            || grayRoi.rows <= 0) {
        BarcodeReadResult result;
        result.status = BarcodeReadStatus::InvalidRoi;
        result.errorReason = "Invalid grayscale barcode ROI";
        return result;
    }

    cv::Mat sourceGray = grayRoi;
    if (!sourceGray.isContinuous()) {
        sourceGray = sourceGray.clone();
    }

    const int maxDecodeTimeMs = std::max(1, options.maxDecodeTimeMs);
    QElapsedTimer timer;
    timer.start();

    const auto isTerminalResult = [](const BarcodeReadResult &value) {
        return value.readable
            || value.status == BarcodeReadStatus::InvalidRoi
            || value.status == BarcodeReadStatus::DecoderUnavailable
            || value.status == BarcodeReadStatus::InternalError;
    };

    BarcodeReadResult result;
    int attemptCount = 0;
    QString lastAttemptName;
    unsigned int lastOptionFlags = BARCODE_DECODER_OPTION_NONE;

    const auto budgetAvailable = [&]() {
        return elapsedMilliseconds(timer) < maxDecodeTimeMs;
    };

    const auto runAttempt = [&](const cv::Mat &candidate,
                                unsigned int optionFlags,
                                int strategyId,
                                const QString &attemptName,
                                double scaleX,
                                double scaleY,
                                double offsetX,
                                double offsetY) -> bool {
        if (!budgetAvailable() || candidate.empty()) {
            return false;
        }

        cv::Mat continuousCandidate = candidate;
        if (continuousCandidate.type() != CV_8UC1) {
            continuousCandidate.convertTo(continuousCandidate, CV_8UC1);
        }
        if (!continuousCandidate.isContinuous()) {
            continuousCandidate = continuousCandidate.clone();
        }

        result = decodeOnce(
                    continuousCandidate,
                    options.formatMask,
                    optionFlags);
        ++attemptCount;
        lastAttemptName = attemptName;
        lastOptionFlags = optionFlags;
        result.elapsedMs = elapsedMilliseconds(timer);

        if (result.readable
                && (std::abs(scaleX - 1.0) > 0.0001
                    || std::abs(scaleY - 1.0) > 0.0001
                    || std::abs(offsetX) > 0.0001
                    || std::abs(offsetY) > 0.0001)) {
            for (cv::Point2f &corner : result.cornersInRoi) {
                corner.x = static_cast<float>(
                    (corner.x - offsetX) / scaleX);
                corner.y = static_cast<float>(
                    (corner.y - offsetY) / scaleY);
            }
        }

        if (result.readable && attemptCount > 1) {
            qDebug() << "[BARCODE_DECODE]"
                     << "fallbackSuccess=" << attemptName
                     << "optionFlags=" << optionFlags
                     << "attempts=" << attemptCount
                     << "elapsedMs=" << result.elapsedMs;
        }
        if (result.readable && successfulStrategyId) {
            *successfulStrategyId = strategyId;
        }
        if (result.readable && successfulOptionFlags) {
            *successfulOptionFlags = optionFlags;
        }

        return isTerminalResult(result);
    };

    if (runAttempt(
                sourceGray,
                BARCODE_DECODER_OPTION_NONE,
                0,
                "original-fast",
                1.0,
                1.0,
                0.0,
                0.0)) {
        return result;
    }

    if (!options.enableFallback) {
        return result;
    }

    const unsigned int normalFallbackOptions =
            BARCODE_DECODER_OPTION_TRY_HARDER;
    const unsigned int invertedFallbackOptions =
            BARCODE_DECODER_OPTION_TRY_HARDER
            | BARCODE_DECODER_OPTION_TRY_INVERT;
    const unsigned int fullFallbackOptions =
            BARCODE_DECODER_OPTION_TRY_HARDER
            | BARCODE_DECODER_OPTION_TRY_INVERT
            | BARCODE_DECODER_OPTION_TRY_ROTATE;

    const auto runFallbackStrategy =
            [&](int strategyId,
                unsigned int optionFlags) -> bool {
        if (!budgetAvailable()) {
            return false;
        }

        switch (strategyId) {
        case 1:
            return runAttempt(
                        sourceGray,
                        optionFlags,
                        1,
                        "original-robust",
                        1.0,
                        1.0,
                        0.0,
                        0.0);
        case 2: {
            const int shorterSide =
                    std::min(sourceGray.cols, sourceGray.rows);
            const int cornerSize =
                    std::max(2, std::min(16, shorterSide / 12));
            const cv::Rect topLeft(
                        0,
                        0,
                        std::min(cornerSize, sourceGray.cols),
                        std::min(cornerSize, sourceGray.rows));
            const cv::Rect topRight(
                        std::max(0, sourceGray.cols - cornerSize),
                        0,
                        std::min(cornerSize, sourceGray.cols),
                        std::min(cornerSize, sourceGray.rows));
            const cv::Rect bottomLeft(
                        0,
                        std::max(0, sourceGray.rows - cornerSize),
                        std::min(cornerSize, sourceGray.cols),
                        std::min(cornerSize, sourceGray.rows));
            const cv::Rect bottomRight(
                        std::max(0, sourceGray.cols - cornerSize),
                        std::max(0, sourceGray.rows - cornerSize),
                        std::min(cornerSize, sourceGray.cols),
                        std::min(cornerSize, sourceGray.rows));
            std::array<double, 4> cornerMeans = {
                cv::mean(sourceGray(topLeft))[0],
                cv::mean(sourceGray(topRight))[0],
                cv::mean(sourceGray(bottomLeft))[0],
                cv::mean(sourceGray(bottomRight))[0]
            };
            std::sort(cornerMeans.begin(), cornerMeans.end());
            const double estimatedBackground =
                    (cornerMeans[1] + cornerMeans[2]) * 0.5;
            const int padding =
                    std::max(
                        4,
                        std::min(
                            20,
                            cvRound(shorterSide * 0.04)));
            cv::Mat padded;
            cv::copyMakeBorder(
                        sourceGray,
                        padded,
                        padding,
                        padding,
                        padding,
                        padding,
                        cv::BORDER_CONSTANT,
                        cv::Scalar(estimatedBackground));
            return runAttempt(
                        padded,
                        optionFlags,
                        2,
                        "estimated-quiet-zone",
                        1.0,
                        1.0,
                        padding,
                        padding);
        }
        case 3: {
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(
                        2.0,
                        cv::Size(8, 8));
            cv::Mat claheImage;
            clahe->apply(sourceGray, claheImage);
            return runAttempt(
                        claheImage,
                        optionFlags,
                        3,
                        "clahe",
                        1.0,
                        1.0,
                        0.0,
                        0.0);
        }
        case 4: {
            cv::Mat medianImage;
            cv::medianBlur(sourceGray, medianImage, 3);
            cv::Mat normalizedImage;
            cv::normalize(
                        medianImage,
                        normalizedImage,
                        0,
                        255,
                        cv::NORM_MINMAX);
            return runAttempt(
                        normalizedImage,
                        optionFlags,
                        4,
                        "median-normalized",
                        1.0,
                        1.0,
                        0.0,
                        0.0);
        }
        case 5: {
            const int shorterSide =
                    std::min(sourceGray.cols, sourceGray.rows);
            int blockSize =
                    std::max(
                        21,
                        std::min(
                            51,
                            cvRound(shorterSide / 8.0)));
            if ((blockSize & 1) == 0) {
                ++blockSize;
            }
            const int largestValidBlock =
                    (shorterSide & 1) == 0
                    ? shorterSide - 1
                    : shorterSide;
            blockSize = std::min(blockSize, largestValidBlock);
            if (blockSize < 3) {
                return false;
            }

            cv::Mat adaptiveImage;
            cv::adaptiveThreshold(
                        sourceGray,
                        adaptiveImage,
                        255,
                        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                        cv::THRESH_BINARY,
                        blockSize,
                        5);
            return runAttempt(
                        adaptiveImage,
                        optionFlags,
                        5,
                        "adaptive-threshold",
                        1.0,
                        1.0,
                        0.0,
                        0.0);
        }
        case 6: {
            cv::Mat downscaled;
            cv::resize(
                        sourceGray,
                        downscaled,
                        cv::Size(),
                        0.75,
                        0.75,
                        cv::INTER_AREA);
            return runAttempt(
                        downscaled,
                        optionFlags,
                        6,
                        "scale-0.75",
                        0.75,
                        0.75,
                        0.0,
                        0.0);
        }
        case 7: {
            cv::Mat upscaled;
            cv::resize(
                        sourceGray,
                        upscaled,
                        cv::Size(),
                        1.5,
                        1.5,
                        cv::INTER_CUBIC);
            return runAttempt(
                        upscaled,
                        optionFlags,
                        7,
                        "scale-1.5",
                        1.5,
                        1.5,
                        0.0,
                        0.0);
        }
        default:
            return false;
        }
    };

    try {
        const int preferredFallbackStrategy =
                preferredStrategyId >= 1
                && preferredStrategyId <= 7
                ? preferredStrategyId
                : -1;

        const unsigned int supportedOptionMask =
                BARCODE_DECODER_OPTION_TRY_HARDER
                | BARCODE_DECODER_OPTION_TRY_INVERT
                | BARCODE_DECODER_OPTION_TRY_ROTATE;
        unsigned int cachedOptionFlags =
                preferredOptionFlags & supportedOptionMask;
        if (preferredFallbackStrategy >= 1) {
            cachedOptionFlags |= BARCODE_DECODER_OPTION_TRY_HARDER;
            if (runFallbackStrategy(
                        preferredFallbackStrategy,
                        cachedOptionFlags)) {
                return result;
            }
        }

        const auto runPhase =
                [&](unsigned int phaseOptionFlags,
                    int phaseDeadlineMs) -> bool {
            for (int strategyId = 1;
                 strategyId <= 7
                 && budgetAvailable()
                 && elapsedMilliseconds(timer) < phaseDeadlineMs;
                 ++strategyId) {
                const bool preferredAlreadyCovered =
                        strategyId == preferredFallbackStrategy
                        && (cachedOptionFlags & phaseOptionFlags)
                           == phaseOptionFlags;
                if (preferredAlreadyCovered) {
                    continue;
                }
                if (runFallbackStrategy(
                            strategyId,
                            phaseOptionFlags)) {
                    return true;
                }
            }
            return false;
        };

        const int normalPhaseDeadlineMs =
                std::max(
                    1,
                    maxDecodeTimeMs * 60 / 100);
        const int invertedPhaseDeadlineMs =
                std::max(
                    normalPhaseDeadlineMs,
                    maxDecodeTimeMs * 85 / 100);

        if (runPhase(
                    normalFallbackOptions,
                    normalPhaseDeadlineMs)) {
            return result;
        }
        if (runPhase(
                    invertedFallbackOptions,
                    invertedPhaseDeadlineMs)) {
            return result;
        }
        if (runPhase(
                    fullFallbackOptions,
                    maxDecodeTimeMs)) {
            return result;
        }
    } catch (const cv::Exception &exception) {
        result.status = BarcodeReadStatus::InternalError;
        result.readable = false;
        result.elapsedMs = elapsedMilliseconds(timer);
        result.errorReason =
                QString("Barcode generic preprocessing failed: %1")
                .arg(QString::fromLocal8Bit(exception.what()));
        return result;
    }

    result.elapsedMs = elapsedMilliseconds(timer);
    if (!result.readable && result.elapsedMs >= maxDecodeTimeMs) {
        result.status = BarcodeReadStatus::Timeout;
        result.errorReason =
                QString("Barcode decoding exceeded %1 ms after %2 attempts")
                .arg(maxDecodeTimeMs)
                .arg(attemptCount);
    } else if (!result.readable) {
        result.status = BarcodeReadStatus::NotFound;
        result.errorReason =
                QString("Barcode was not found after %1 generic attempts")
                .arg(attemptCount);
    }

    qDebug() << "[BARCODE_DECODE]"
             << "readable=" << result.readable
             << "attempts=" << attemptCount
             << "lastAttempt=" << lastAttemptName
             << "lastOptionFlags=" << lastOptionFlags
             << "elapsedMs=" << result.elapsedMs
             << "status=" << static_cast<int>(result.status);
    return result;
}
