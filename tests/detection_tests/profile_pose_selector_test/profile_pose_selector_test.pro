QT += core testlib
QT -= gui

TEMPLATE = app
TARGET = profile_pose_selector_test
CONFIG += console testcase c++11
CONFIG -= app_bundle

# This target sits three shadow-build levels below the repository root.  jom
# can otherwise misinterpret qmake's generated relative source dependencies.
QMAKE_PROJECT_DEPTH = 0

PROJECT_ROOT = $$clean_path($$PWD/../../..)
THIRD_PARTY = $$PROJECT_ROOT/third_party

DEFINES += CV_IGNORE_DEBUG_BUILD_GUARD

SOURCES += \
    profile_pose_selector_test.cpp \
    $$PROJECT_ROOT/app/detection/common/profile_pose_selector.cpp

HEADERS += \
    $$PROJECT_ROOT/app/detection/common/profile_pose_selector.h \
    $$PROJECT_ROOT/app/detection/positioning/detection_pose.h

INCLUDEPATH += \
    $$PROJECT_ROOT/app \
    $$THIRD_PARTY/opencv/include \
    $$THIRD_PARTY/opencv/x64/vc15/include

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS_DEBUG += /MTd
}

CONFIG(release, debug|release) {
    QMAKE_CXXFLAGS_RELEASE += /MT
}

win32:CONFIG(release, debug|release): LIBS += \
    -L$$THIRD_PARTY/opencv/x64/vc15/lib/ \
    -lopencv_world341
else:win32:CONFIG(debug, debug|release): LIBS += \
    -L$$THIRD_PARTY/opencv/x64/vc15/lib/ \
    -lopencv_world341d

win32 {
    TEST_RUNTIME_DEPLOY_SCRIPT = $$shell_path($$PROJECT_ROOT/tests/deploy_test_runtime.ps1)
    TEST_QT_RUNTIME_DIR = $$clean_path($$[QT_INSTALL_BINS])

    CONFIG(release, debug|release) {
        TEST_OPENCV_RUNTIME = $$shell_path($$PROJECT_ROOT/dist/ShengYin/opencv_world341.dll)
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Core.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Test.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/release)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/release/profile_pose_selector_test.exe)
    }

    CONFIG(debug, debug|release) {
        TEST_OPENCV_RUNTIME = $$shell_path($$THIRD_PARTY/opencv/x64/vc15/bin/opencv_world341d.dll)
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Cored.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Testd.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/debug)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/debug/profile_pose_selector_test.exe)
    }

    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$TEST_RUNTIME_DEPLOY_SCRIPT -SourceDll $$TEST_OPENCV_RUNTIME -QtCoreDll $$TEST_QT_CORE_RUNTIME -QtTestDll $$TEST_QT_TEST_RUNTIME -TargetExecutable $$TEST_TARGET_EXECUTABLE -Destination $$TEST_RUNTIME_DESTINATION
}
