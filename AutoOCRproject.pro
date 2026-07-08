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
    CameraThread.cpp \
    Detector.cpp \
    ImageManipulator.cpp \
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
    CameraThread.h \
    CryptoUtils.h \
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
    imageManipulator.h \
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
QMAKE_LFLAGS += /MTd

TRANSLATIONS += Translate_EN.ts \
                 Translate_CN.ts \




RC_ICONS = sy.ico

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target



INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\paddle\fluid\inference
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\paddle\include
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\protobuf\include
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\glog\include
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\gflags\include
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\xxhash\include
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\zlib\include
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\third_party\boost
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\third_party\eigen3
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\mklml\include
INCLUDEPATH += $$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\mkldnn\include
INCLUDEPATH += $$PWD\..\3rdparty\opencv\include
                $$PWD\include\


LIBS += -L$$PWD\..\3rdparty\paddle_inference_install_dir\paddle\lib -lpaddle_inference
LIBS += -L$$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\mklml\lib -lmklml
LIBS += -L$$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\mklml\lib -llibiomp5md
LIBS += -L$$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\mkldnn\lib -lmkldnn
LIBS += -L$$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\glog\lib -lglog
LIBS += -L$$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\gflags\lib -lgflags_static
LIBS += -L$$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\protobuf\lib -llibprotobuf
LIBS += -L$$PWD\..\3rdparty\paddle_inference_install_dir\third_party\install\xxhash\lib -lxxhash
#LIBS += -L$$PWD\..\3rdparty\opencv -lopencv_world440



#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/SDK/Lib/ -lMvCameraControl
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/SDK/Lib/ -lMvCameraControld

INCLUDEPATH += $$PWD/SDK/Includes
DEPENDPATH += $$PWD/SDK/Includes


RC_ICONS = sy.ico

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/OpenCV/ -lopencv_world440

INCLUDEPATH += $$PWD/OpenCV
DEPENDPATH += $$PWD/OpenCV
INCLUDEPATH += $$PWD/Bin
DEPENDPATH += $$PWD/Bin
INCLUDEPATH += $$PWD/../3rdparty/opencv/include/opencv2
DEPENDPATH += $$PWD/../3rdparty/opencv/include/opencv2
INCLUDEPATH += $$PWD/../3rdparty/Libraries/win64
DEPENDPATH += $$PWD/../3rdparty/Libraries/win64



LIBS += -L$$PWD/../3rdparty/Libraries/win64/ -lMvCameraControl
#LIBS += -L$$PWD/../3rdparty/Libraries/win64/ -lopencv_world440
LIBS += -L$$PWD/../3rdparty/Libraries/win64/ -lsnap7
#LIBS += -L$$PWD/../3rdparty/Libraries/win64/ -lopencv_world4100



INCLUDEPATH += $$PWD/../3rdparty/opencv/x64/vc15/bin
DEPENDPATH += $$PWD/../3rdparty/opencv/x64/vc15/bin
INCLUDEPATH += $$PWD/../3rdparty/opencv/x64/vc15/lib
DEPENDPATH += $$PWD/../3rdparty/opencv/x64/vc15/lib
LIBS += -lopencv_core341 \
        -lopencv_imgproc341 \
        -lopencv_highgui341 \
        -lopencv_tracking341 \
        -lopencv_videoio341 \
        -lopencv_objdetect341
        -lopencv_features2d
        -lopencv_xfeatures2d

LIBS += -L$$PWD/../3rdparty/opencv/x64/vc15/lib/ -lopencv_tracking341
LIBS += -L$$PWD/../3rdparty/opencv/x64/vc15/lib/ -lopencv_tracking341



win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../3rdparty/opencv/x64/vc15/lib/ -lopencv_img_hash341
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../3rdparty/opencv/x64/vc15/lib/ -lopencv_img_hash341d

INCLUDEPATH += $$PWD/../3rdparty/opencv/x64/vc15/include
DEPENDPATH += $$PWD/../3rdparty/opencv/x64/vc15/include

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../3rdparty/opencv/x64/vc15/lib/ -lopencv_world341
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../3rdparty/opencv/x64/vc15/lib/ -lopencv_world341d

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../3rdparty/opencv/x64/vc15/lib/ -lopencv_xfeatures2d341
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../3rdparty/opencv/x64/vc15/lib/ -lopencv_xfeatures2d341d

INCLUDEPATH += $$PWD/../3rdparty/opencv/x64/vc15/include
DEPENDPATH += $$PWD/../3rdparty/opencv/x64/vc15/include

DISTFILES +=
