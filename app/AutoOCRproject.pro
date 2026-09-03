QT       += core gui network svg

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

msvc {
    QMAKE_CXXFLAGS += /utf-8
}


TARGET = ShengYin
TEMPLATE = app

# The .pro file lives in app/. All repository-level paths are derived from
# this one location so Qt Creator kits do not depend on the former nested root.
PROJECT_ROOT = $$clean_path($$PWD/..)
THIRD_PARTY = $$PROJECT_ROOT/third_party

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += CV_IGNORE_DEBUG_BUILD_GUARD

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS_DEBUG += /MTd
}

CONFIG(release, debug|release) {
    QMAKE_CXXFLAGS_RELEASE += /MT
}

SOURCES += \
    application/inspection_application_service.cpp \
    application/inspection_start_preflight.cpp \
    application/settings_application_service.cpp \
    application/template_application_service.cpp \
    application/template_geometry_service.cpp \
    templates/template_store.cpp \
    ui/main_window/template/selection/template_selection_dialog.cpp \
    ui/main_window/template/save/template_save_dialog.cpp \
    ui/main_window/template/character_editor/character_template_editor_dialog.cpp \
    ui/main_window/template/character_editor/character_crop_label.cpp \
    ui/main_window/inspection/inspection_page.cpp \
    ui/main_window/settings/machine_settings_page.cpp \
    ui/main_window/operation_ui_policy.cpp \
    ui/main_window/settings/settings_edit_state.cpp \
    ui/main_window/template/template_editor_page.cpp \
    system_support/machine_settings_policy.cpp \
    system_support/settings/app_settings.cpp \
    system_support/settings/app_settings_store.cpp \
    system_support/license/license_codec.cpp \
    system_support/logging/application_logger.cpp \
    system_support/logging/log_categories.cpp \
    system_support/crash/windows_crash_stack.cpp \
    system_support/crash/windows_crash_handler.cpp \
    startup/runtime_guard.cpp \
    startup/single_instance_guard.cpp \
    startup/application_startup.cpp \
    ui/main_window/inspection/inspection_fault_presenter.cpp \
    runtime/inspection_presentation_renderer.cpp \
    engines/barcode/vendor/barcode_decoder_adapter.cpp \
    devices/camera/vendor/hikvision_camera_device.cpp \
    engines/ocr/vendor/paddle_ocr_engine.cpp \
    devices/plc/vendor/snap7_plc_device.cpp \
    runtime/camera_session.cpp \
    runtime/capture_worker.cpp \
    runtime/frame_queue.cpp \
    runtime/detection_worker.cpp \
    detection/detection_registry.cpp \
    detection/multi_template_runtime_snapshot.cpp \
    runtime/result_presentation_mailbox.cpp \
    runtime/inspection_plc_controller.cpp \
    runtime/inspection_runtime.cpp \
    runtime/result_service.cpp \
    runtime/result_export_client.cpp \
    runtime/result_export_network_worker.cpp \
    runtime/image_save_service.cpp \
    detection/common/character_template_matcher.cpp \
    detection/common/frame_preprocessor.cpp \
    detection/common/template_pose_selector.cpp \
    detection/common/inspection_positioner.cpp \
    detection/common/tracking_pose_matcher.cpp \
    detection/detectionmode/ocr/ocr_detection_pipeline.cpp \
    detection/detectionmode/barcode_word/barcode_word_detection_pipeline.cpp \
    detection/detectionmode/stamp/stamp_detection_pipeline.cpp \
    detection/detectionmode/tissue/tissue_detection_pipeline.cpp \
    detection/detectionmode/word/word_detection_pipeline.cpp \
    contracts/detection_mode.cpp \
    detection/detectionmode/stamp/overlap_detector.cpp \
    engines/ocr/vendor/paddle/src/clipper.cpp \
    engines/ocr/vendor/paddle/src/config.cpp \
    engines/ocr/vendor/paddle/src/ocr_cls.cpp \
    engines/ocr/vendor/paddle/src/ocr_det.cpp \
    engines/ocr/vendor/paddle/src/ocr_rec.cpp \
    engines/ocr/vendor/paddle/src/postprocess_op.cpp \
    engines/ocr/vendor/paddle/src/preprocess_op.cpp \
    engines/ocr/vendor/paddle/src/utility.cpp \
    detection/detectionmode/tissue/tissue_roll_detector.cpp \
    ui/main_window/inspection_image_canvas.cpp \
    startup/main.cpp \
    devices/plc/vendor/snap7.cpp \
    ui/main_window/main_window.cpp \
    ui/main_window/main_window_inspection.cpp \
    ui/main_window/main_window_settings.cpp

HEADERS += \
    application/application_result.h \
    application/camera_application_contract.h \
    application/inspection_application_service.h \
    application/inspection_start_preflight.h \
    application/runtime_snapshot.h \
    application/settings_application_service.h \
    application/template_application_service.h \
    application/template_geometry_service.h \
    contracts/barcode_parameter_defaults.h \
    templates/template_store.h \
    ui/main_window/template/selection/template_selection_dialog.h \
    ui/main_window/template/save/template_save_dialog.h \
    ui/main_window/template/character_editor/character_template_editor_dialog.h \
    ui/main_window/template/character_editor/character_crop_label.h \
    contracts/inspection_presentation.h \
    runtime/result_service.h \
    runtime/result_export_client.h \
    runtime/result_export_network_worker.h \
    ui/main_window/inspection/inspection_page.h \
    ui/main_window/settings/machine_settings_page.h \
    ui/main_window/operation_ui_policy.h \
    ui/main_window/settings/settings_edit_state.h \
    ui/main_window/template/template_editor_page.h \
    system_support/machine_settings_policy.h \
    system_support/settings/app_settings.h \
    system_support/settings/app_settings_store.h \
    system_support/license/license_codec.h \
    system_support/logging/application_logger.h \
    system_support/logging/log_categories.h \
    system_support/crash/windows_crash_stack.h \
    system_support/crash/windows_crash_handler.h \
    startup/runtime_guard.h \
    startup/single_instance_guard.h \
    startup/application_startup.h \
    ui/main_window/inspection/inspection_fault_presenter.h \
    runtime/inspection_presentation_renderer.h \
    engines/barcode/vendor/barcode_decoder_adapter.h \
    engines/barcode/barcode_decoder.h \
    devices/camera/camera_device.h \
    devices/camera/vendor/hikvision_camera_device.h \
    engines/ocr/ocr_engine.h \
    engines/ocr/vendor/paddle_ocr_engine.h \
    devices/plc/plc_device.h \
    devices/plc/vendor/snap7_plc_device.h \
    runtime/camera_session.h \
    runtime/capture_worker.h \
    runtime/frame_queue.h \
    runtime/detection_worker.h \
    detection/detection_registry.h \
    detection/multi_template_runtime_snapshot.h \
    application/template_editor_contract.h \
    runtime/result_presentation_mailbox.h \
    runtime/inspection_plc_controller.h \
    runtime/inspection_runtime.h \
    runtime/image_save_service.h \
    detection/common/character_template_matcher.h \
    detection/common/detection_roi_geometry.h \
    detection/common/frame_preprocessor.h \
    detection/common/template_pose_selector.h \
    detection/common/inspection_positioner.h \
    detection/common/tracking_pose_matcher.h \
    detection/detectionmode/ocr/ocr_detection_pipeline.h \
    detection/detectionmode/barcode_word/barcode_word_detection_pipeline.h \
    detection/detectionmode/stamp/stamp_detection_pipeline.h \
    detection/detectionmode/tissue/tissue_detection_pipeline.h \
    detection/detectionmode/word/word_detection_pipeline.h \
    engines/barcode/vendor/barcode_decoder_api.h \
    engines/barcode/barcode_types.h \
    contracts/detection_mode.h \
    detection/detectionmode/stamp/overlap_detector.h \
    engines/ocr/vendor/paddle/include/clipper.h \
    engines/ocr/vendor/paddle/include/config.h \
    engines/ocr/vendor/paddle/include/ocr_cls.h \
    engines/ocr/vendor/paddle/include/ocr_det.h \
    engines/ocr/vendor/paddle/include/ocr_rec.h \
    engines/ocr/vendor/paddle/include/postprocess_op.h \
    engines/ocr/vendor/paddle/include/preprocess_op.h \
    engines/ocr/vendor/paddle/include/utility.h \
    detection/common/detection_pose.h \
    detection/detectionmode/tissue/tissue_roll_detector.h \
    ui/main_window/inspection_image_canvas.h \
    devices/plc/vendor/snap7.h \
    ui/main_window/main_window.h

FORMS += \
    ui/main_window/main_window.ui \
    ui/main_window/inspection/inspection_info_page.ui \
    ui/main_window/settings/detection_settings_page.ui \
    ui/main_window/settings/image_settings_page.ui \
    ui/main_window/settings/plc_settings_page.ui \
    ui/main_window/settings/software_settings_page.ui \
    ui/main_window/template/selection/template_selection_dialog.ui \
    ui/main_window/template/save/template_save_dialog.ui \
    ui/main_window/template/character_editor/character_template_editor_dialog.ui

RESOURCES += \
    resource/image.qrc


TRANSLATIONS += resource/Translate_EN.ts \
                resource/Translate_CN.ts

RC_ICONS = resource/sy.ico

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
INCLUDEPATH += $$THIRD_PARTY/paddle_inference_install_dir/paddle/include
INCLUDEPATH += $$THIRD_PARTY/paddle_inference_install_dir/third_party/install/protobuf/include
INCLUDEPATH += $$THIRD_PARTY/paddle_inference_install_dir/third_party/install/glog/include
INCLUDEPATH += $$THIRD_PARTY/paddle_inference_install_dir/third_party/install/gflags/include
INCLUDEPATH += $$THIRD_PARTY/paddle_inference_install_dir/third_party/install/xxhash/include
INCLUDEPATH += $$THIRD_PARTY/paddle_inference_install_dir/third_party/install/mklml/include
INCLUDEPATH += $$THIRD_PARTY/paddle_inference_install_dir/third_party/install/mkldnn/include
INCLUDEPATH += $$THIRD_PARTY/opencv/include


LIBS += -L$$THIRD_PARTY/paddle_inference_install_dir/paddle/lib -lpaddle_inference
LIBS += -L$$THIRD_PARTY/paddle_inference_install_dir/third_party/install/mklml/lib -lmklml
LIBS += -L$$THIRD_PARTY/paddle_inference_install_dir/third_party/install/mklml/lib -llibiomp5md
LIBS += -L$$THIRD_PARTY/paddle_inference_install_dir/third_party/install/mkldnn/lib -lmkldnn
LIBS += -L$$THIRD_PARTY/paddle_inference_install_dir/third_party/install/glog/lib -lglog
LIBS += -L$$THIRD_PARTY/paddle_inference_install_dir/third_party/install/gflags/lib -lgflags_static
LIBS += -L$$THIRD_PARTY/paddle_inference_install_dir/third_party/install/protobuf/lib -llibprotobuf
LIBS += -L$$THIRD_PARTY/paddle_inference_install_dir/third_party/install/xxhash/lib -lxxhash



INCLUDEPATH += $$THIRD_PARTY/hikvision_mvs_sdk/include
DEPENDPATH += $$THIRD_PARTY/hikvision_mvs_sdk/include
INCLUDEPATH += $$THIRD_PARTY/Libraries/win64
DEPENDPATH += $$THIRD_PARTY/Libraries/win64
LIBS += -L$$THIRD_PARTY/hikvision_mvs_sdk/lib/win64/ -lMvCameraControl
LIBS += -L$$THIRD_PARTY/Libraries/win64/ -lsnap7

INCLUDEPATH += $$THIRD_PARTY/opencv/x64/vc15/include
DEPENDPATH += $$THIRD_PARTY/opencv/x64/vc15/include

win32:CONFIG(release, debug|release): LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_world341
else:win32:CONFIG(debug, debug|release): LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_world341d

# Let Qt Creator run the Release executable from its build directory. The
# validated dist package provides non-Qt runtime assets; the active Qt kit
# deploys its matching Qt libraries and plugins.
win32:CONFIG(release, debug|release) {
    RUNTIME_DEPLOY_SCRIPT = $$shell_path($$PROJECT_ROOT/app/system_support/deployment/deploy_runtime.ps1)
    RUNTIME_DEPLOY_SOURCE = $$shell_path($$PROJECT_ROOT/dist/ShengYin)
    RUNTIME_DEPLOY_DESTINATION = $$shell_path($$OUT_PWD/release)
    RUNTIME_DEPLOY_QT_BIN = $$shell_path($$[QT_INSTALL_BINS])
    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$quote($$RUNTIME_DEPLOY_SCRIPT) -Source $$quote($$RUNTIME_DEPLOY_SOURCE) -Destination $$quote($$RUNTIME_DEPLOY_DESTINATION) -QtBinDirectory $$quote($$RUNTIME_DEPLOY_QT_BIN)
}
