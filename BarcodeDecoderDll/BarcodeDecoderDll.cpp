#include "BarcodeDecoderApi.h"

#include <Barcode.h>
#include <BarcodeFormat.h>
#include <ImageView.h>
#include <ReadBarcode.h>
#include <ReaderOptions.h>
#include <ZXingCpp.h>

#include <dmtx.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;

constexpr int LibDmtxFallbackTimeMs = 15;

bool hasFlag(unsigned int value, unsigned int flag)
{
    return (value & flag) != 0u;
}

bool checkedImageSize(int height, int stride)
{
    if (height <= 0 || stride <= 0) {
        return false;
    }

    const size_t rowCount = static_cast<size_t>(height);
    const size_t rowStride = static_cast<size_t>(stride);
    return rowCount <= std::numeric_limits<size_t>::max() / rowStride;
}

ZXing::BarcodeFormats toZxingFormats(unsigned int formatMask)
{
    std::vector<ZXing::BarcodeFormat> formats;
    formats.reserve(2);

    if (hasFlag(formatMask, BARCODE_DECODER_FORMAT_DATA_MATRIX)) {
        formats.push_back(ZXing::BarcodeFormat::DataMatrix);
    }
    if (hasFlag(formatMask, BARCODE_DECODER_FORMAT_QR_CODE)) {
        formats.push_back(ZXing::BarcodeFormat::QRCode);
    }

    return ZXing::BarcodeFormats(std::move(formats));
}

int toDecoderFormat(ZXing::BarcodeFormat format)
{
    if (format <= ZXing::BarcodeFormat::DataMatrix) {
        return BARCODE_DECODER_FORMAT_DATA_MATRIX;
    }
    if (format <= ZXing::BarcodeFormat::QRCode) {
        return BARCODE_DECODER_FORMAT_QR_CODE;
    }
    return 0;
}

void clearOutputs(
    char *utf8Text,
    int textCapacity,
    int *textLength,
    int *decodedFormat,
    float corners[8],
    int *elapsedMicroseconds)
{
    if (utf8Text && textCapacity > 0) {
        utf8Text[0] = '\0';
    }
    if (textLength) {
        *textLength = 0;
    }
    if (decodedFormat) {
        *decodedFormat = 0;
    }
    if (corners) {
        std::fill(corners, corners + 8, 0.0f);
    }
    if (elapsedMicroseconds) {
        *elapsedMicroseconds = 0;
    }
}

void setElapsed(
    const Clock::time_point &startedAt,
    int *elapsedMicroseconds)
{
    if (!elapsedMicroseconds) {
        return;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - startedAt).count();
    *elapsedMicroseconds = static_cast<int>(
        std::min<long long>(
            elapsed,
            std::numeric_limits<int>::max()));
}

unsigned char medianOfNine(std::array<unsigned char, 9> values)
{
    for (size_t index = 1; index < values.size(); ++index) {
        const unsigned char value = values[index];
        size_t insertAt = index;
        while (insertAt > 0
               && values[insertAt - 1] > value) {
            values[insertAt] = values[insertAt - 1];
            --insertAt;
        }
        values[insertAt] = value;
    }
    return values[4];
}

bool makeMedianNormalizedImage(
    const unsigned char *data,
    int width,
    int height,
    int stride,
    std::vector<unsigned char> *output)
{
    if (!output
            || width <= 0
            || height <= 0
            || stride < width) {
        return false;
    }

    const size_t pixelCount =
        static_cast<size_t>(width) * static_cast<size_t>(height);
    output->resize(pixelCount);

    unsigned char minimum = 255;
    unsigned char maximum = 0;
    for (int y = 0; y < height; ++y) {
        const unsigned char *sourceRow =
            data + static_cast<size_t>(y) * stride;
        unsigned char *targetRow =
            output->data() + static_cast<size_t>(y) * width;

        for (int x = 0; x < width; ++x) {
            unsigned char value = sourceRow[x];
            if (x > 0
                    && x + 1 < width
                    && y > 0
                    && y + 1 < height) {
                const unsigned char *previousRow =
                    data + static_cast<size_t>(y - 1) * stride;
                const unsigned char *nextRow =
                    data + static_cast<size_t>(y + 1) * stride;
                value = medianOfNine({
                    previousRow[x - 1],
                    previousRow[x],
                    previousRow[x + 1],
                    sourceRow[x - 1],
                    sourceRow[x],
                    sourceRow[x + 1],
                    nextRow[x - 1],
                    nextRow[x],
                    nextRow[x + 1]
                });
            }

            targetRow[x] = value;
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }
    }

    if (maximum <= minimum) {
        return true;
    }

    const unsigned int range =
        static_cast<unsigned int>(maximum - minimum);
    for (unsigned char &value : *output) {
        const unsigned int shifted =
            static_cast<unsigned int>(value - minimum);
        value = static_cast<unsigned char>(
            (shifted * 255u + range / 2u) / range);
    }
    return true;
}

struct DmtxDecodeResources
{
    DmtxImage *image = nullptr;
    DmtxDecode *decoder = nullptr;
    DmtxRegion *region = nullptr;
    DmtxMessage *message = nullptr;

    ~DmtxDecodeResources()
    {
        if (message) {
            dmtxMessageDestroy(&message);
        }
        if (region) {
            dmtxRegionDestroy(&region);
        }
        if (decoder) {
            dmtxDecodeDestroy(&decoder);
        }
        if (image) {
            dmtxImageDestroy(&image);
        }
    }

    void clearCandidate()
    {
        if (message) {
            dmtxMessageDestroy(&message);
        }
        if (region) {
            dmtxRegionDestroy(&region);
        }
    }
};

struct FloatPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

void writeLibDmtxCorners(
    DmtxRegion *region,
    int width,
    int height,
    float corners[8])
{
    std::array<DmtxVector2, 4> fitted = {{
        {0.0, 0.0},
        {1.0, 0.0},
        {1.0, 1.0},
        {0.0, 1.0}
    }};
    std::array<FloatPoint, 4> points;

    for (size_t index = 0; index < fitted.size(); ++index) {
        dmtxMatrix3VMultiplyBy(
            &fitted[index],
            region->fit2raw);
        points[index].x = static_cast<float>(
            std::clamp(fitted[index].X, 0.0, width - 1.0));
        points[index].y = static_cast<float>(
            std::clamp(
                height - 1.0 - fitted[index].Y,
                0.0,
                height - 1.0));
    }

    const auto sum = [](const FloatPoint &point) {
        return point.x + point.y;
    };
    const auto difference = [](const FloatPoint &point) {
        return point.x - point.y;
    };

    const FloatPoint &topLeft =
        *std::min_element(
            points.begin(),
            points.end(),
            [&](const FloatPoint &left, const FloatPoint &right) {
                return sum(left) < sum(right);
            });
    const FloatPoint &bottomRight =
        *std::max_element(
            points.begin(),
            points.end(),
            [&](const FloatPoint &left, const FloatPoint &right) {
                return sum(left) < sum(right);
            });
    const FloatPoint &topRight =
        *std::max_element(
            points.begin(),
            points.end(),
            [&](const FloatPoint &left, const FloatPoint &right) {
                return difference(left) < difference(right);
            });
    const FloatPoint &bottomLeft =
        *std::min_element(
            points.begin(),
            points.end(),
            [&](const FloatPoint &left, const FloatPoint &right) {
                return difference(left) < difference(right);
            });

    const std::array<FloatPoint, 4> ordered = {
        topLeft,
        topRight,
        bottomRight,
        bottomLeft
    };
    for (size_t index = 0; index < ordered.size(); ++index) {
        corners[index * 2] = ordered[index].x;
        corners[index * 2 + 1] = ordered[index].y;
    }
}

bool tryLibDmtxFallback(
    const unsigned char *data,
    int width,
    int height,
    int stride,
    char *utf8Text,
    int textCapacity,
    int *textLength,
    int *decodedFormat,
    float corners[8])
{
    const Clock::time_point fallbackStartedAt = Clock::now();
    std::vector<unsigned char> preparedImage;
    if (!makeMedianNormalizedImage(
            data,
            width,
            height,
            stride,
            &preparedImage)) {
        return false;
    }

    DmtxDecodeResources resources;
    resources.image = dmtxImageCreate(
        preparedImage.data(),
        width,
        height,
        DmtxPack8bppK);
    if (!resources.image) {
        return false;
    }

    resources.decoder =
        dmtxDecodeCreate(resources.image, 1);
    if (!resources.decoder) {
        return false;
    }

    const long long preprocessingMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - fallbackStartedAt).count();
    if (preprocessingMs >= LibDmtxFallbackTimeMs) {
        return false;
    }
    const long remainingTimeMs =
        static_cast<long>(
            LibDmtxFallbackTimeMs - preprocessingMs);
    DmtxTime deadline = dmtxTimeAdd(
        dmtxTimeNow(),
        remainingTimeMs);
    while ((resources.region =
                dmtxRegionFindNext(
                    resources.decoder,
                    &deadline)) != nullptr) {
        resources.message = dmtxDecodeMatrixRegion(
            resources.decoder,
            resources.region,
            DmtxUndefined);
        if (resources.message
                && resources.message->output
                && resources.message->outputIdx > 0) {
            const int outputLength =
                resources.message->outputIdx;
            if (outputLength > textCapacity) {
                return false;
            }

            std::memcpy(
                utf8Text,
                resources.message->output,
                static_cast<size_t>(outputLength));
            if (outputLength < textCapacity) {
                utf8Text[outputLength] = '\0';
            }
            *textLength = outputLength;
            *decodedFormat =
                BARCODE_DECODER_FORMAT_DATA_MATRIX;
            writeLibDmtxCorners(
                resources.region,
                width,
                height,
                corners);
            return true;
        }
        resources.clearCandidate();
    }

    return false;
}

} // namespace

extern "C" int BARCODE_DECODER_CALL BarcodeDecoder_GetVersion(
    char *utf8Buffer,
    int bufferCapacity)
{
    if (!utf8Buffer || bufferCapacity <= 0) {
        return BARCODE_DECODER_ERROR_INVALID_ARGUMENT;
    }

    const std::string version =
        std::string("BarcodeDecoder/2.1.0 ZXing-C++/")
        + ZXing::Version()
        + " libdmtx/0.7.8";
    if (version.size() + 1u > static_cast<size_t>(bufferCapacity)) {
        utf8Buffer[0] = '\0';
        return BARCODE_DECODER_ERROR_INVALID_ARGUMENT;
    }

    std::memcpy(
        utf8Buffer,
        version.c_str(),
        version.size() + 1u);
    return static_cast<int>(version.size());
}

extern "C" int BARCODE_DECODER_CALL BarcodeDecoder_DecodeLuma8(
    const unsigned char *data,
    int width,
    int height,
    int stride,
    unsigned int formatMask,
    unsigned int optionFlags,
    char *utf8Text,
    int textCapacity,
    int *textLength,
    int *decodedFormat,
    float corners[8],
    int *elapsedMicroseconds)
{
    const Clock::time_point startedAt = Clock::now();
    clearOutputs(
        utf8Text,
        textCapacity,
        textLength,
        decodedFormat,
        corners,
        elapsedMicroseconds);

    if (!data
            || width <= 0
            || height <= 0
            || stride < width
            || !checkedImageSize(height, stride)
            || !utf8Text
            || textCapacity <= 0
            || !textLength
            || !decodedFormat
            || !corners
            || !elapsedMicroseconds) {
        setElapsed(startedAt, elapsedMicroseconds);
        return BARCODE_DECODER_ERROR_INVALID_ARGUMENT;
    }

    const unsigned int supportedFormats =
        BARCODE_DECODER_FORMAT_DATA_MATRIX
        | BARCODE_DECODER_FORMAT_QR_CODE;
    if ((formatMask & supportedFormats) == 0u) {
        setElapsed(startedAt, elapsedMicroseconds);
        return BARCODE_DECODER_ERROR_UNSUPPORTED_FORMAT;
    }

    try {
        const bool legacyFullFallback =
            hasFlag(
                optionFlags,
                BARCODE_DECODER_OPTION_TRY_HARDER)
            && hasFlag(
                optionFlags,
                BARCODE_DECODER_OPTION_TRY_ROTATE)
            && hasFlag(
                optionFlags,
                BARCODE_DECODER_OPTION_TRY_INVERT);

        ZXing::ReaderOptions options;
        options.formats(toZxingFormats(formatMask));
        options.maxNumberOfSymbols(1);
        options.tryHarder(
            hasFlag(optionFlags, BARCODE_DECODER_OPTION_TRY_HARDER));
        options.tryRotate(
            hasFlag(optionFlags, BARCODE_DECODER_OPTION_TRY_ROTATE));
        options.tryInvert(
            hasFlag(optionFlags, BARCODE_DECODER_OPTION_TRY_INVERT));
        options.tryDownscale(
            legacyFullFallback
            || hasFlag(
                optionFlags,
                BARCODE_DECODER_OPTION_TRY_DOWNSCALE));
        options.tryDenoise(
            legacyFullFallback
            || hasFlag(
                optionFlags,
                BARCODE_DECODER_OPTION_TRY_DENOISE));
        options.binarizer(
            hasFlag(
                optionFlags,
                BARCODE_DECODER_OPTION_GLOBAL_HISTOGRAM)
            ? ZXing::Binarizer::GlobalHistogram
            : ZXing::Binarizer::LocalAverage);
        options.isPure(false);
        options.returnErrors(false);
        options.textMode(ZXing::TextMode::Plain);

        const ZXing::ImageView image(
            data,
            width,
            height,
            ZXing::ImageFormat::Lum,
            stride);
        const ZXing::Barcode barcode =
            ZXing::ReadBarcode(image, options);

        if (!barcode.isValid()
                && hasFlag(
                    formatMask,
                    BARCODE_DECODER_FORMAT_DATA_MATRIX)
                && hasFlag(
                    optionFlags,
                    BARCODE_DECODER_OPTION_TRY_HARDER)
                && tryLibDmtxFallback(
                    data,
                    width,
                    height,
                    stride,
                    utf8Text,
                    textCapacity,
                    textLength,
                    decodedFormat,
                    corners)) {
            setElapsed(startedAt, elapsedMicroseconds);
            return BARCODE_DECODER_RESULT_SUCCESS;
        }

        if (!barcode.isValid()) {
            setElapsed(startedAt, elapsedMicroseconds);
            return BARCODE_DECODER_RESULT_NOT_FOUND;
        }

        const std::vector<uint8_t> &bytes = barcode.bytes();
        if (bytes.empty()
                || bytes.size()
                   > static_cast<size_t>(textCapacity)) {
            setElapsed(startedAt, elapsedMicroseconds);
            return BARCODE_DECODER_ERROR_INTERNAL;
        }

        std::memcpy(
            utf8Text,
            bytes.data(),
            bytes.size());
        if (bytes.size() < static_cast<size_t>(textCapacity)) {
            utf8Text[bytes.size()] = '\0';
        }
        *textLength = static_cast<int>(bytes.size());
        *decodedFormat = toDecoderFormat(barcode.format());

        const ZXing::Position &position = barcode.position();
        for (size_t index = 0; index < 4; ++index) {
            corners[index * 2] =
                static_cast<float>(position[index].x);
            corners[index * 2 + 1] =
                static_cast<float>(position[index].y);
        }

        setElapsed(startedAt, elapsedMicroseconds);
        return BARCODE_DECODER_RESULT_SUCCESS;
    } catch (const std::exception &) {
        setElapsed(startedAt, elapsedMicroseconds);
        return BARCODE_DECODER_ERROR_INTERNAL;
    } catch (...) {
        setElapsed(startedAt, elapsedMicroseconds);
        return BARCODE_DECODER_ERROR_INTERNAL;
    }
}
