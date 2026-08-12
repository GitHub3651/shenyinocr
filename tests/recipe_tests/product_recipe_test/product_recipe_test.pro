QT += core testlib
QT -= gui

TEMPLATE = app
TARGET = product_recipe_test
CONFIG += console testcase c++11
CONFIG -= app_bundle

QMAKE_PROJECT_DEPTH = 0

PROJECT_ROOT = $$clean_path($$PWD/../../..)

SOURCES += \
    ../product_recipe_test.cpp \
    $$PROJECT_ROOT/app/recipes/product_recipe.cpp \
    $$PROJECT_ROOT/app/recipes/template_profile_assets.cpp \
    $$PROJECT_ROOT/app/recipes/template_recipe_assembler.cpp \
    $$PROJECT_ROOT/app/recipes/template_profile_mapper.cpp

HEADERS += \
    $$PROJECT_ROOT/app/recipes/product_recipe.h \
    $$PROJECT_ROOT/app/recipes/template_profile_assets.h \
    $$PROJECT_ROOT/app/recipes/template_recipe_assembler.h \
    $$PROJECT_ROOT/app/recipes/template_profile_mapper.h

INCLUDEPATH += \
    $$PROJECT_ROOT/app \
    $$PROJECT_ROOT/app/recipes \
    $$PROJECT_ROOT/third_party/opencv/include

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
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Core.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Test.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/release)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/release/product_recipe_test.exe)
    }

    CONFIG(debug, debug|release) {
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Cored.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Testd.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/debug)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/debug/product_recipe_test.exe)
    }

    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$TEST_RUNTIME_DEPLOY_SCRIPT -QtCoreDll $$TEST_QT_CORE_RUNTIME -QtTestDll $$TEST_QT_TEST_RUNTIME -TargetExecutable $$TEST_TARGET_EXECUTABLE -Destination $$TEST_RUNTIME_DESTINATION
}
