QT += core gui testlib

TEMPLATE = app
TARGET = application_service_test
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
    application_service_test.cpp \
    $$PROJECT_ROOT/app/DetectionModes.cpp \
    $$PROJECT_ROOT/app/application/inspection_application_service.cpp \
    $$PROJECT_ROOT/app/application/inspection_start_preflight.cpp \
    $$PROJECT_ROOT/app/application/settings_application_service.cpp \
    $$PROJECT_ROOT/app/recipes/product_recipe.cpp \
    $$PROJECT_ROOT/app/recipes/recipe_store.cpp \
    $$PROJECT_ROOT/app/recipes/prepared_recipe.cpp \
    $$PROJECT_ROOT/app/runtime/camera_session.cpp \
    $$PROJECT_ROOT/app/runtime/capture_worker.cpp \
    $$PROJECT_ROOT/app/runtime/detection_worker.cpp \
    $$PROJECT_ROOT/app/runtime/frame_queue.cpp \
    $$PROJECT_ROOT/app/runtime/image_save_service.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_plc_controller.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_profile_snapshot.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_run_configuration.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_runtime.cpp \
    $$PROJECT_ROOT/app/runtime/pipeline_registry.cpp \
    $$PROJECT_ROOT/app/runtime/result_service.cpp \
    $$PROJECT_ROOT/app/runtime/result_presentation_mailbox.cpp \
    $$PROJECT_ROOT/app/system_support/settings/machine_settings.cpp \
    $$PROJECT_ROOT/app/system_support/settings/machine_settings_store.cpp \
    $$PROJECT_ROOT/app/detection/common/character_template_matcher.cpp \
    $$PROJECT_ROOT/app/detection/common/frame_preprocessor.cpp \
    $$PROJECT_ROOT/app/detection/common/profile_pose_selector.cpp \
    $$PROJECT_ROOT/app/detection/positioning/tracking_pose_matcher.cpp \
    $$PROJECT_ROOT/app/detection/positioning/inspection_positioner.cpp \
    $$PROJECT_ROOT/app/detection/ocr/ocr_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/detection/barcode_word/barcode_word_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/detection/stamp/stamp_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/detection/tissue/tissue_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/detection/word/word_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_presentation_renderer.cpp \
    $$PROJECT_ROOT/app/Detector.cpp \
    $$PROJECT_ROOT/app/TissueRollDetector.cpp

HEADERS += \
    $$PROJECT_ROOT/app/DetectionModes.h \
    $$PROJECT_ROOT/app/application/application_result.h \
    $$PROJECT_ROOT/app/application/inspection_application_service.h \
    $$PROJECT_ROOT/app/application/inspection_start_preflight.h \
    $$PROJECT_ROOT/app/application/runtime_snapshot.h \
    $$PROJECT_ROOT/app/application/settings_application_service.h \
    $$PROJECT_ROOT/app/contracts/barcode_parameter_defaults.h \
    $$PROJECT_ROOT/app/recipes/product_recipe.h \
    $$PROJECT_ROOT/app/recipes/recipe_store.h \
    $$PROJECT_ROOT/app/recipes/prepared_recipe.h \
    $$PROJECT_ROOT/app/devices/camera/camera_device.h \
    $$PROJECT_ROOT/app/runtime/camera_session.h \
    $$PROJECT_ROOT/app/runtime/capture_worker.h \
    $$PROJECT_ROOT/app/runtime/detection_worker.h \
    $$PROJECT_ROOT/app/runtime/frame_queue.h \
    $$PROJECT_ROOT/app/runtime/image_save_service.h \
    $$PROJECT_ROOT/app/runtime/inspection_plc_controller.h \
    $$PROJECT_ROOT/app/runtime/inspection_profile_snapshot.h \
    $$PROJECT_ROOT/app/runtime/inspection_run_configuration.h \
    $$PROJECT_ROOT/app/runtime/inspection_run_context.h \
    $$PROJECT_ROOT/app/runtime/inspection_runtime.h \
    $$PROJECT_ROOT/app/runtime/pipeline_registry.h \
    $$PROJECT_ROOT/app/runtime/inspection_presentation.h \
    $$PROJECT_ROOT/app/runtime/result_service.h \
    $$PROJECT_ROOT/app/runtime/result_presentation_mailbox.h \
    $$PROJECT_ROOT/app/system_support/settings/machine_settings.h \
    $$PROJECT_ROOT/app/system_support/settings/machine_settings_store.h \
    $$PROJECT_ROOT/app/devices/barcode/barcode_decoder.h \
    $$PROJECT_ROOT/app/devices/ocr/ocr_engine.h \
    $$PROJECT_ROOT/app/devices/plc/plc_device.h \
    $$PROJECT_ROOT/app/detection/common/character_template_matcher.h \
    $$PROJECT_ROOT/app/detection/common/frame_preprocessor.h \
    $$PROJECT_ROOT/app/detection/common/profile_pose_selector.h \
    $$PROJECT_ROOT/app/detection/positioning/tracking_pose_matcher.h \
    $$PROJECT_ROOT/app/detection/positioning/inspection_positioner.h \
    $$PROJECT_ROOT/app/detection/ocr/ocr_detection_pipeline.h \
    $$PROJECT_ROOT/app/detection/barcode_word/barcode_word_detection_pipeline.h \
    $$PROJECT_ROOT/app/detection/stamp/stamp_detection_pipeline.h \
    $$PROJECT_ROOT/app/detection/tissue/tissue_detection_pipeline.h \
    $$PROJECT_ROOT/app/detection/word/word_detection_pipeline.h \
    $$PROJECT_ROOT/app/runtime/inspection_presentation_renderer.h \
    $$PROJECT_ROOT/app/Detector.h \
    $$PROJECT_ROOT/app/TissueRollDetector.h

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
        TEST_QT_GUI_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Gui.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Test.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/release)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/release/application_service_test.exe)
    }

    CONFIG(debug, debug|release) {
        TEST_OPENCV_RUNTIME = $$shell_path($$THIRD_PARTY/opencv/x64/vc15/bin/opencv_world341d.dll)
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Cored.dll)
        TEST_QT_GUI_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Guid.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Testd.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/debug)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/debug/application_service_test.exe)
    }

    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$TEST_RUNTIME_DEPLOY_SCRIPT -SourceDll $$TEST_OPENCV_RUNTIME -QtCoreDll $$TEST_QT_CORE_RUNTIME -QtGuiDll $$TEST_QT_GUI_RUNTIME -QtTestDll $$TEST_QT_TEST_RUNTIME -TargetExecutable $$TEST_TARGET_EXECUTABLE -Destination $$TEST_RUNTIME_DESTINATION
}
