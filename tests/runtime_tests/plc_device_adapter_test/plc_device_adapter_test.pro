QT += core testlib
QT -= gui

TEMPLATE = app
TARGET = plc_device_adapter_test
CONFIG += console testcase c++11
CONFIG -= app_bundle

QMAKE_PROJECT_DEPTH = 0

PROJECT_ROOT = $$clean_path($$PWD/../../..)
THIRD_PARTY = $$PROJECT_ROOT/third_party

SOURCES += \
    plc_device_adapter_test.cpp \
    $$PROJECT_ROOT/app/devices/plc/vendor/snap7_plc_device.cpp \
    $$PROJECT_ROOT/app/devices/plc/vendor/snap7.cpp

HEADERS += \
    $$PROJECT_ROOT/app/devices/plc/plc_device.h \
    $$PROJECT_ROOT/app/devices/plc/vendor/snap7_plc_device.h \
    $$PROJECT_ROOT/app/devices/plc/vendor/snap7.h

INCLUDEPATH += \
    $$PROJECT_ROOT/app \
    $$PROJECT_ROOT/app/devices/plc

LIBS += -L$$THIRD_PARTY/Libraries/win64/ -lsnap7

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS_DEBUG += /MTd
}

CONFIG(release, debug|release) {
    QMAKE_CXXFLAGS_RELEASE += /MT
}

win32 {
    TEST_RUNTIME_DEPLOY_SCRIPT = $$shell_path($$PROJECT_ROOT/tests/deploy_test_runtime.ps1)
    TEST_QT_RUNTIME_DIR = $$clean_path($$[QT_INSTALL_BINS])
    TEST_SNAP7_RUNTIME = $$shell_path($$PROJECT_ROOT/dist/ShengYin/snap7.dll)

    CONFIG(release, debug|release) {
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Core.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Test.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/release)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/release/plc_device_adapter_test.exe)
    }

    CONFIG(debug, debug|release) {
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Cored.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Testd.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/debug)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/debug/plc_device_adapter_test.exe)
    }

    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$TEST_RUNTIME_DEPLOY_SCRIPT -SourceDll $$TEST_SNAP7_RUNTIME -QtCoreDll $$TEST_QT_CORE_RUNTIME -QtTestDll $$TEST_QT_TEST_RUNTIME -TargetExecutable $$TEST_TARGET_EXECUTABLE -Destination $$TEST_RUNTIME_DESTINATION
}
