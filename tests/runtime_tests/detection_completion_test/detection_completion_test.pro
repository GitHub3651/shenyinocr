QT += core gui testlib

TEMPLATE = app
TARGET = detection_completion_test
CONFIG += console testcase c++11
CONFIG -= app_bundle

QMAKE_PROJECT_DEPTH = 0

PROJECT_ROOT = $$clean_path($$PWD/../../..)
THIRD_PARTY = $$PROJECT_ROOT/third_party

DEFINES += CV_IGNORE_DEBUG_BUILD_GUARD

SOURCES += \
    detection_completion_test.cpp \
    $$PROJECT_ROOT/app/runtime/detection_shadow_comparator.cpp \
    $$PROJECT_ROOT/app/runtime/frame_queue.cpp \
    $$PROJECT_ROOT/app/runtime/detection_worker.cpp \
    $$PROJECT_ROOT/app/runtime/detection_mode_worker_factory.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_profile_snapshot.cpp \
    $$PROJECT_ROOT/app/runtime/result_presentation_mailbox.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_fault_state.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_product_reconciler.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_run_configuration.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_acquisition_stop_coordinator.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_camera_start_transition.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_camera_recovery_transition.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_start_preflight.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_runtime_controller.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_runtime_start_transaction.cpp \
    $$PROJECT_ROOT/app/runtime/inspection_runtime_stop_transaction.cpp \
    $$PROJECT_ROOT/app/runtime/detection_session.cpp \
    $$PROJECT_ROOT/app/runtime/result_handler.cpp \
    $$PROJECT_ROOT/app/runtime/image_save_service.cpp \
    $$PROJECT_ROOT/app/ui/controllers/detection_completion_controller.cpp \
    $$PROJECT_ROOT/app/ui/controllers/operation_ui_policy.cpp \
    $$PROJECT_ROOT/app/ui/controllers/settings_edit_state.cpp \
    $$PROJECT_ROOT/app/ui/presenters/inspection_fault_presenter.cpp \
    $$PROJECT_ROOT/app/detection/common/character_template_matcher.cpp \
    $$PROJECT_ROOT/app/detection/ocr/ocr_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/detection/barcode_word/barcode_word_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/detection/stamp/stamp_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/detection/tissue/tissue_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/detection/word/word_detection_pipeline.cpp \
    $$PROJECT_ROOT/app/TissueRollDetector.cpp \
    $$PROJECT_ROOT/app/ui/presenters/detection_result_presenter.cpp

HEADERS += \
    $$PROJECT_ROOT/app/TrackingTypes.h \
    $$PROJECT_ROOT/app/runtime/detection_shadow_comparator.h \
    $$PROJECT_ROOT/app/runtime/frame_queue.h \
    $$PROJECT_ROOT/app/runtime/detection_worker.h \
    $$PROJECT_ROOT/app/runtime/detection_mode_worker_factory.h \
    $$PROJECT_ROOT/app/runtime/inspection_profile_snapshot.h \
    $$PROJECT_ROOT/app/runtime/result_presentation_mailbox.h \
    $$PROJECT_ROOT/app/runtime/inspection_fault_state.h \
    $$PROJECT_ROOT/app/runtime/inspection_product_reconciler.h \
    $$PROJECT_ROOT/app/runtime/inspection_run_configuration.h \
    $$PROJECT_ROOT/app/runtime/inspection_acquisition_stop_coordinator.h \
    $$PROJECT_ROOT/app/runtime/inspection_camera_start_transition.h \
    $$PROJECT_ROOT/app/runtime/inspection_camera_recovery_transition.h \
    $$PROJECT_ROOT/app/runtime/inspection_start_preflight.h \
    $$PROJECT_ROOT/app/runtime/inspection_runtime_controller.h \
    $$PROJECT_ROOT/app/runtime/inspection_runtime_start_transaction.h \
    $$PROJECT_ROOT/app/runtime/inspection_runtime_stop_transaction.h \
    $$PROJECT_ROOT/app/runtime/detection_session.h \
    $$PROJECT_ROOT/app/runtime/result_handler.h \
    $$PROJECT_ROOT/app/runtime/image_save_service.h \
    $$PROJECT_ROOT/app/ui/controllers/detection_completion_controller.h \
    $$PROJECT_ROOT/app/ui/controllers/operation_ui_policy.h \
    $$PROJECT_ROOT/app/ui/controllers/settings_edit_state.h \
    $$PROJECT_ROOT/app/ui/presenters/inspection_fault_presenter.h \
    $$PROJECT_ROOT/app/BarcodeDecoderApi.h \
    $$PROJECT_ROOT/app/BarcodeTypes.h \
    $$PROJECT_ROOT/app/TissueRollDetector.h \
    $$PROJECT_ROOT/app/recipes/product_recipe.h \
    $$PROJECT_ROOT/app/devices/barcode/barcode_decoder_adapter.h \
    $$PROJECT_ROOT/app/devices/ocr/ocr_engine.h \
    $$PROJECT_ROOT/app/detection/common/character_template_matcher.h \
    $$PROJECT_ROOT/app/detection/common/detection_roi_geometry.h \
    $$PROJECT_ROOT/app/detection/ocr/ocr_detection_pipeline.h \
    $$PROJECT_ROOT/app/detection/barcode_word/barcode_word_detection_pipeline.h \
    $$PROJECT_ROOT/app/detection/stamp/stamp_detection_pipeline.h \
    $$PROJECT_ROOT/app/detection/tissue/tissue_detection_pipeline.h \
    $$PROJECT_ROOT/app/detection/word/word_detection_pipeline.h \
    $$PROJECT_ROOT/app/ui/presenters/detection_result_presenter.h

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
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/release/detection_completion_test.exe)
    }

    CONFIG(debug, debug|release) {
        TEST_OPENCV_RUNTIME = $$shell_path($$THIRD_PARTY/opencv/x64/vc15/bin/opencv_world341d.dll)
        TEST_QT_CORE_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Cored.dll)
        TEST_QT_GUI_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Guid.dll)
        TEST_QT_TEST_RUNTIME = $$shell_path($$TEST_QT_RUNTIME_DIR/Qt5Testd.dll)
        TEST_RUNTIME_DESTINATION = $$shell_path($$OUT_PWD/debug)
        TEST_TARGET_EXECUTABLE = $$shell_path($$OUT_PWD/debug/detection_completion_test.exe)
    }

    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$TEST_RUNTIME_DEPLOY_SCRIPT -SourceDll $$TEST_OPENCV_RUNTIME -QtCoreDll $$TEST_QT_CORE_RUNTIME -QtGuiDll $$TEST_QT_GUI_RUNTIME -QtTestDll $$TEST_QT_TEST_RUNTIME -TargetExecutable $$TEST_TARGET_EXECUTABLE -Destination $$TEST_RUNTIME_DESTINATION
}
