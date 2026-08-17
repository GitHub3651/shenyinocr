QT += core testlib
QT -= gui

TEMPLATE = app
TARGET = recipe_store_test
CONFIG += console testcase c++11
CONFIG -= app_bundle

QMAKE_PROJECT_DEPTH = 0

PROJECT_ROOT = $$clean_path($$PWD/../../..)

SOURCES += \
    recipe_store_test.cpp \
    $$PROJECT_ROOT/app/contracts/detection_mode.cpp \
    $$PROJECT_ROOT/app/recipes/product_recipe.cpp \
    $$PROJECT_ROOT/app/recipes/recipe_store.cpp \
    $$PROJECT_ROOT/app/recipes/prepared_recipe.cpp \
    $$PROJECT_ROOT/app/recipes/recipe_editor_session.cpp \
    $$PROJECT_ROOT/app/application/inspection_start_preflight.cpp

HEADERS += \
    $$PROJECT_ROOT/app/contracts/barcode_parameter_defaults.h \
    $$PROJECT_ROOT/app/contracts/detection_mode.h \
    $$PROJECT_ROOT/app/recipes/product_recipe.h \
    $$PROJECT_ROOT/app/recipes/recipe_store.h \
    $$PROJECT_ROOT/app/recipes/prepared_recipe.h \
    $$PROJECT_ROOT/app/recipes/recipe_editor_session.h \
    $$PROJECT_ROOT/app/application/inspection_start_preflight.h

INCLUDEPATH += \
    $$PROJECT_ROOT/app \
    $$PROJECT_ROOT/third_party/opencv/include \
    $$PROJECT_ROOT/third_party/opencv/x64/vc15/include

win32:CONFIG(release, debug|release): LIBS += \
    -L$$PROJECT_ROOT/third_party/opencv/x64/vc15/lib/ \
    -lopencv_world341
else:win32:CONFIG(debug, debug|release): LIBS += \
    -L$$PROJECT_ROOT/third_party/opencv/x64/vc15/lib/ \
    -lopencv_world341d

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS_DEBUG += /MTd
}

CONFIG(release, debug|release) {
    QMAKE_CXXFLAGS_RELEASE += /MT
}

win32 {
    TEST_RUNTIME_DEPLOY_SCRIPT = $$shell_path($$PROJECT_ROOT/tests/deploy_test_runtime.ps1)
    TEST_QT_RUNTIME_DIR = $$clean_path($$[QT_INSTALL_BINS])

    CONFIG(release, debug|release) {
        TEST_OPENCV_RUNTIME = $$shell_path($$PROJECT_ROOT/dist/ShengYin/opencv_world341.dll)
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Core.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Test.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/release)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/release/recipe_store_test.exe)
    }

    CONFIG(debug, debug|release) {
        TEST_OPENCV_RUNTIME = $$shell_path($$PROJECT_ROOT/third_party/opencv/x64/vc15/bin/opencv_world341d.dll)
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Cored.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Testd.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/debug)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/debug/recipe_store_test.exe)
    }

    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$TEST_RUNTIME_DEPLOY_SCRIPT -SourceDll $$TEST_OPENCV_RUNTIME -QtCoreDll $$TEST_QT_CORE_RUNTIME -QtTestDll $$TEST_QT_TEST_RUNTIME -TargetExecutable $$TEST_TARGET_EXECUTABLE -Destination $$TEST_RUNTIME_DESTINATION
}
