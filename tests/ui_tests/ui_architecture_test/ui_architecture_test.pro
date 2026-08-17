QT += core gui widgets testlib

TEMPLATE = app
TARGET = ui_architecture_test
CONFIG += console testcase c++11
CONFIG -= app_bundle

msvc {
    QMAKE_CXXFLAGS += /utf-8
}

QMAKE_PROJECT_DEPTH = 0

PROJECT_ROOT = $$clean_path($$PWD/../../..)
THIRD_PARTY = $$PROJECT_ROOT/third_party

DEFINES += CV_IGNORE_DEBUG_BUILD_GUARD

SOURCES += \
    ui_architecture_test.cpp

INCLUDEPATH += \
    $$PROJECT_ROOT/app \
    $$THIRD_PARTY/opencv/include

DEPENDPATH += $$INCLUDEPATH

win32 {
    CONFIG(release, debug|release) {
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/release/ui_architecture_test.exe)
    } else {
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/debug/ui_architecture_test.exe)
    }
    QMAKE_POST_LINK += $$quote(cmd /c if exist $$TEST_TARGET_EXECUTABLE copy /Y $$TEST_TARGET_EXECUTABLE $$shell_path($$PROJECT_ROOT/tests/bin/) >nul)
}
