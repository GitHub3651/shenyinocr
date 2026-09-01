QT += core gui network widgets

CONFIG += c++11
TEMPLATE = app
TARGET = ResultReceiver

msvc {
    QMAKE_CXXFLAGS += /utf-8
}

SOURCES += \
    main.cpp \
    result_receiver_window.cpp \
    result_receiver_server.cpp \
    result_receiver_store.cpp

HEADERS += \
    result_receiver_window.h \
    result_receiver_server.h \
    result_receiver_store.h

FORMS += \
    result_receiver_window.ui

DISTFILES += \
    README.md \
    deploy_result_receiver.ps1

win32:CONFIG(release, debug|release) {
    DEPLOY_SCRIPT = $$shell_path($$PWD/deploy_result_receiver.ps1)
    QT_BIN_DIRECTORY = $$shell_path($$[QT_INSTALL_BINS])
    RELEASE_EXECUTABLE = $$shell_path($$OUT_PWD/release/$${TARGET}.exe)
    QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -File $$quote($$DEPLOY_SCRIPT) -QtBinDirectory $$quote($$QT_BIN_DIRECTORY) -Executable $$quote($$RELEASE_EXECUTABLE)
}
