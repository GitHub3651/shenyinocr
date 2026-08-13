#-------------------------------------------------
#
# Project created by QtCreator 2020-06-25T14:45:44
#
#-------------------------------------------------


QT       += core gui axcontainer serialport sql
QT       += multimedia multimediawidgets
#QT       += xlsx
#QT       += sql


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11


TARGET = ShengYin
TEMPLATE = app

# The .pro file lives in app/. All repository-level paths are derived from
# this one location so Qt Creator kits do not depend on the former nested root.
PROJECT_ROOT = $$clean_path($$PWD/..)
THIRD_PARTY = $$PROJECT_ROOT/third_party

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += CV_IGNORE_DEBUG_BUILD_GUARD
# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS_DEBUG += /MTd
    QMAKE_CFLAGS_RELEASE += -g
    QMAKE_CXXFLAGS_RELEASE += -g
    QMAKE_CFLAGS_RELEASE -= -O2
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_LFLAGS_RELEASE = -mthreads -W
}

CONFIG(release, debug|release) {
    QMAKE_CXXFLAGS_RELEASE += /MT
}

SOURCES += \
    appsettingsmanager.cpp \
    recipes/product_recipe.cpp \
    recipes/recipe_store.cpp \
    recipes/recipe_selection.cpp \
    recipes/template_profile_assets.cpp \
    recipes/template_profile_load_plan.cpp \
    recipes/template_recipe_assembler.cpp \
    recipes/template_recipe_draft_session.cpp \
    recipes/template_recipe_edit_session.cpp \
    recipes/template_recipe_publisher.cpp \
    recipes/template_profile_mapper.cpp \
    ui/dialogs/recipe_selection_dialog.cpp \
    detection/common/profile_pose_selector.cpp \
    detection/ocr/ocr_detection_pipeline.cpp \
    detection/barcode_word/barcode_word_detection_pipeline.cpp \
    detection/stamp/stamp_detection_pipeline.cpp \
    detection/tissue/tissue_detection_pipeline.cpp \
    detection/word/word_detection_pipeline.cpp \
    CameraThread.cpp \
    DetectionModes.cpp \
    Detector.cpp \
    MultiCameraController.cpp \
    MultiCameraSyncManager.cpp \
    MultiCameraUnit.cpp \
    PaddleOCR/src/clipper.cpp \
    PaddleOCR/src/config.cpp \
    PaddleOCR/src/ocr_cls.cpp \
    PaddleOCR/src/ocr_det.cpp \
    PaddleOCR/src/ocr_rec.cpp \
    PaddleOCR/src/postprocess_op.cpp \
    PaddleOCR/src/preprocess_op.cpp \
    PaddleOCR/src/utility.cpp \
    TrackingPoseMatcher.cpp \
    RuntimeGuard.cpp \
    TissueRollDetector.cpp \
    Zhuizong.cpp \
    charactertemplatecropdialog.cpp \
    ccrashstack.cpp \
    cmvcamera.cpp \
    imagelabel.cpp \
        main.cpp \
    multicamerawidget.cpp \
    mythread.cpp \
    snap7.cpp \
    templatematch.cpp \
        widget.cpp

HEADERS += \
    appsettingsmanager.h \
    recipes/product_recipe.h \
    recipes/recipe_store.h \
    recipes/recipe_selection.h \
    recipes/template_profile_assets.h \
    recipes/template_profile_load_plan.h \
    recipes/template_recipe_assembler.h \
    recipes/template_recipe_draft_session.h \
    recipes/template_recipe_edit_session.h \
    recipes/template_recipe_publisher.h \
    recipes/template_profile_mapper.h \
    ui/dialogs/recipe_selection_dialog.h \
    detection/common/profile_pose_selector.h \
    detection/ocr/ocr_detection_pipeline.h \
    detection/barcode_word/barcode_word_detection_pipeline.h \
    detection/stamp/stamp_detection_pipeline.h \
    detection/tissue/tissue_detection_pipeline.h \
    detection/word/word_detection_pipeline.h \
    BarcodeDecoderApi.h \
    BarcodeTypes.h \
    CameraThread.h \
    DetectionModes.h \
    Detector.h \
    IMultiCameraProvider.h \
    MultiCameraController.h \
    MultiCameraSyncManager.h \
    MultiCameraTypes.h \
    MultiCameraUnit.h \
    PaddleOCR/include/clipper.h \
    PaddleOCR/include/config.h \
    PaddleOCR/include/ocr_cls.h \
    PaddleOCR/include/ocr_det.h \
    PaddleOCR/include/ocr_rec.h \
    PaddleOCR/include/postprocess_op.h \
    PaddleOCR/include/preprocess_op.h \
    PaddleOCR/include/utility.h \
    TrackingPoseMatcher.h \
    TrackingTypes.h \
    RuntimeGuard.h \
    TissueRollDetector.h \
    Zhuizong.h \
    charactertemplatecropdialog.h \
    ccrashstack.h \
    cmvcamera.h \
    imagelabel.h \
    multicamerawidget.h \
    mythread.h \
    snap7.h \
    templatematch.h \
        widget.h

FORMS += \
        multicamerawidget.ui \
        widget.ui

RESOURCES += \
    image/image.qrc


CONFIG += C++11

TRANSLATIONS += Translate_EN.ts \
                 Translate_CN.ts \




RC_ICONS = sy.ico

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



#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/SDK/Lib/ -lMvCameraControl
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/SDK/Lib/ -lMvCameraControld

INCLUDEPATH += $$THIRD_PARTY/hikvision_mvs_sdk/include
DEPENDPATH += $$THIRD_PARTY/hikvision_mvs_sdk/include


RC_ICONS = sy.ico

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target



INCLUDEPATH += $$THIRD_PARTY/opencv/include/opencv2
DEPENDPATH += $$THIRD_PARTY/opencv/include/opencv2
INCLUDEPATH += $$THIRD_PARTY/Libraries/win64
DEPENDPATH += $$THIRD_PARTY/Libraries/win64



LIBS += -L$$THIRD_PARTY/hikvision_mvs_sdk/lib/win64/ -lMvCameraControl
LIBS += -L$$THIRD_PARTY/Libraries/win64/ -lsnap7



INCLUDEPATH += $$THIRD_PARTY/opencv/x64/vc15/bin
DEPENDPATH += $$THIRD_PARTY/opencv/x64/vc15/bin
INCLUDEPATH += $$THIRD_PARTY/opencv/x64/vc15/lib
DEPENDPATH += $$THIRD_PARTY/opencv/x64/vc15/lib
LIBS += -lopencv_core341 \
        -lopencv_imgproc341 \
        -lopencv_highgui341 \
        -lopencv_tracking341 \
        -lopencv_videoio341 \
        -lopencv_objdetect341
        -lopencv_features2d
        -lopencv_xfeatures2d

LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_tracking341
LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_tracking341



win32:CONFIG(release, debug|release): LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_img_hash341
else:win32:CONFIG(debug, debug|release): LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_img_hash341d

INCLUDEPATH += $$THIRD_PARTY/opencv/x64/vc15/include
DEPENDPATH += $$THIRD_PARTY/opencv/x64/vc15/include

win32:CONFIG(release, debug|release): LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_world341
else:win32:CONFIG(debug, debug|release): LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_world341d

win32:CONFIG(release, debug|release): LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_xfeatures2d341
else:win32:CONFIG(debug, debug|release): LIBS += -L$$THIRD_PARTY/opencv/x64/vc15/lib/ -lopencv_xfeatures2d341d

INCLUDEPATH += $$THIRD_PARTY/opencv/x64/vc15/include
DEPENDPATH += $$THIRD_PARTY/opencv/x64/vc15/include

DISTFILES +=

# Let Qt Creator run the Release executable from its build directory.  The
# validated dist package remains the source of runtime DLLs, models and config.
win32:CONFIG(release, debug|release) {
    RUNTIME_DEPLOY_SCRIPT = $$shell_path($$PROJECT_ROOT/app/deploy_runtime.ps1)
    RUNTIME_DEPLOY_SOURCE = $$shell_path($$PROJECT_ROOT/dist/ShengYin)
    RUNTIME_DEPLOY_DESTINATION = $$shell_path($$OUT_PWD/release)
    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$RUNTIME_DEPLOY_SCRIPT -Source $$RUNTIME_DEPLOY_SOURCE -Destination $$RUNTIME_DEPLOY_DESTINATION
}
