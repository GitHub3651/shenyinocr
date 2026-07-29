#ifndef BARCODEDECODERAPI_H
#define BARCODEDECODERAPI_H

/*
 * BarcodeDecoder.dll public C ABI.
 *
 * The main Qt application loads these functions through QLibrary, so this
 * header must not expose Qt, OpenCV or C++ standard-library types.
 */

#if defined(_WIN32)
#define BARCODE_DECODER_CALL __cdecl
#if defined(BARCODE_DECODER_EXPORTS)
#define BARCODE_DECODER_API __declspec(dllexport)
#else
#define BARCODE_DECODER_API
#endif
#else
#define BARCODE_DECODER_CALL
#define BARCODE_DECODER_API
#endif

enum BarcodeDecoderFormatMask
{
    BARCODE_DECODER_FORMAT_DATA_MATRIX = 1u << 0,
    BARCODE_DECODER_FORMAT_QR_CODE = 1u << 1
};

enum BarcodeDecoderOptionFlag
{
    BARCODE_DECODER_OPTION_NONE = 0u,
    BARCODE_DECODER_OPTION_TRY_HARDER = 1u << 0,
    BARCODE_DECODER_OPTION_TRY_ROTATE = 1u << 1,
    BARCODE_DECODER_OPTION_TRY_INVERT = 1u << 2,

    // The following flags are supported by BarcodeDecoder.dll 2.0 and later.
    // They only extend the option mask; the exported C function ABI is unchanged.
    BARCODE_DECODER_OPTION_TRY_DOWNSCALE = 1u << 3,
    BARCODE_DECODER_OPTION_TRY_DENOISE = 1u << 4,
    BARCODE_DECODER_OPTION_GLOBAL_HISTOGRAM = 1u << 5
};

enum BarcodeDecoderReturnCode
{
    BARCODE_DECODER_RESULT_NOT_FOUND = 0,
    BARCODE_DECODER_RESULT_SUCCESS = 1,
    BARCODE_DECODER_ERROR_INVALID_ARGUMENT = -1,
    BARCODE_DECODER_ERROR_INTERNAL = -2,
    BARCODE_DECODER_ERROR_UNSUPPORTED_FORMAT = -3
};

#ifdef __cplusplus
extern "C" {
#endif

BARCODE_DECODER_API int BARCODE_DECODER_CALL BarcodeDecoder_GetVersion(
    char *utf8Buffer,
    int bufferCapacity);

BARCODE_DECODER_API int BARCODE_DECODER_CALL BarcodeDecoder_DecodeLuma8(
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
    int *elapsedMicroseconds);

#ifdef __cplusplus
}
#endif

typedef int (BARCODE_DECODER_CALL *BarcodeDecoderGetVersionFunction)(
    char *utf8Buffer,
    int bufferCapacity);

typedef int (BARCODE_DECODER_CALL *BarcodeDecoderDecodeLuma8Function)(
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
    int *elapsedMicroseconds);

#endif // BARCODEDECODERAPI_H
