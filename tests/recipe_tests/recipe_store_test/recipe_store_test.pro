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
    $$PROJECT_ROOT/app/recipes/product_recipe.cpp \
    $$PROJECT_ROOT/app/recipes/recipe_store.cpp \
    $$PROJECT_ROOT/app/recipes/recipe_selection.cpp \
    $$PROJECT_ROOT/app/recipes/template_profile_load_plan.cpp \
    $$PROJECT_ROOT/app/recipes/template_recipe_assembler.cpp \
    $$PROJECT_ROOT/app/recipes/template_recipe_draft_session.cpp \
    $$PROJECT_ROOT/app/recipes/template_recipe_publisher.cpp

HEADERS += \
    $$PROJECT_ROOT/app/recipes/product_recipe.h \
    $$PROJECT_ROOT/app/recipes/recipe_store.h \
    $$PROJECT_ROOT/app/recipes/recipe_selection.h \
    $$PROJECT_ROOT/app/recipes/template_profile_load_plan.h \
    $$PROJECT_ROOT/app/recipes/template_profile_assets.h \
    $$PROJECT_ROOT/app/recipes/template_recipe_assembler.h \
    $$PROJECT_ROOT/app/recipes/template_recipe_draft_session.h \
    $$PROJECT_ROOT/app/recipes/template_recipe_publisher.h

INCLUDEPATH += \
    $$PROJECT_ROOT/app/recipes

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
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/release/recipe_store_test.exe)
    }

    CONFIG(debug, debug|release) {
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Cored.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Testd.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/debug)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/debug/recipe_store_test.exe)
    }

    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$TEST_RUNTIME_DEPLOY_SCRIPT -QtCoreDll $$TEST_QT_CORE_RUNTIME -QtTestDll $$TEST_QT_TEST_RUNTIME -TargetExecutable $$TEST_TARGET_EXECUTABLE -Destination $$TEST_RUNTIME_DESTINATION
}
