cmake_minimum_required(VERSION 3.20)

project(BarcodeDecoder VERSION 2.1.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "BarcodeDecoder.dll is a Windows-only component.")
endif()

include(FetchContent)

# Build a small, self-contained reader DLL. ZXing-C++ is pinned so rebuilding
# this project later cannot silently change production decoding behavior.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ZXING_READERS ON CACHE BOOL "" FORCE)
set(ZXING_WRITERS OFF CACHE STRING "" FORCE)
set(ZXING_C_API OFF CACHE BOOL "" FORCE)
set(ZXING_EXPERIMENTAL_API ON CACHE BOOL "" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZXING_EXAMPLES_QT OFF CACHE BOOL "" FORCE)
set(ZXING_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_TEST_DOTNET OFF CACHE BOOL "" FORCE)
set(ZXING_TEST_GO OFF CACHE BOOL "" FORCE)
set(ZXING_TEST_INSTALL OFF CACHE BOOL "" FORCE)
set(ZXING_PYTHON_MODULE OFF CACHE BOOL "" FORCE)
set(ZXING_LINK_CPP_STATICALLY ON CACHE BOOL "" FORCE)

set(ZXING_ENABLE_1D OFF CACHE BOOL "" FORCE)
set(ZXING_ENABLE_AZTEC OFF CACHE BOOL "" FORCE)
set(ZXING_ENABLE_DATAMATRIX ON CACHE BOOL "" FORCE)
set(ZXING_ENABLE_MAXICODE OFF CACHE BOOL "" FORCE)
set(ZXING_ENABLE_PDF417 OFF CACHE BOOL "" FORCE)
set(ZXING_ENABLE_QRCODE ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    zxing_cpp
    URL
        https://github.com/zxing-cpp/zxing-cpp/releases/download/v3.1.0-rc1/zxing-cpp-3.1.0-rc1.zip
    URL_HASH
        SHA256=BF7273103B2B25494BA82074DAF9967F306BB257C441153E95DA7F8D35F116B0
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(zxing_cpp)

# libdmtx is used only as a time-limited Data Matrix fallback after ZXing
# fails. It is statically linked so the deployed application still needs
# only BarcodeDecoder.dll.
set(DMTX_SHARED OFF CACHE BOOL "" FORCE)
set(DMTX_STATIC ON CACHE BOOL "" FORCE)
set(DMTX_READER_PROGRAMMING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    libdmtx
    URL
        https://github.com/dmtx/libdmtx/archive/refs/tags/v0.7.8.zip
    URL_HASH
        SHA256=3E79CC187028623890ECAF366A4A2E0AFC07941ACBD3705C5F7149F972726737
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(libdmtx)

add_library(BarcodeDecoder SHARED
    BarcodeDecoderDll.cpp
    ../BarcodeDecoderApi.h
)

target_compile_features(BarcodeDecoder PRIVATE cxx_std_20)
target_compile_definitions(BarcodeDecoder PRIVATE
    BARCODE_DECODER_EXPORTS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
target_compile_options(BarcodeDecoder PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)
target_include_directories(BarcodeDecoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${libdmtx_SOURCE_DIR}
)
target_link_libraries(BarcodeDecoder PRIVATE
    ZXing::Core
    dmtx_static
)

set_property(
    TARGET BarcodeDecoder
    PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
set_property(
    TARGET dmtx_static
    PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
set_target_properties(BarcodeDecoder PROPERTIES
    OUTPUT_NAME "BarcodeDecoder"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
)
