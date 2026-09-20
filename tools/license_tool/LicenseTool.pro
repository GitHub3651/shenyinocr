QT += core gui widgets

CONFIG += c++11
CONFIG -= app_bundle

msvc {
    QMAKE_CXXFLAGS += /utf-8
}

TEMPLATE = app
TARGET = LicenseTool

DESTDIR = $$OUT_PWD

APP_ROOT = $$clean_path($$PWD/../../app)
INCLUDEPATH += $$APP_ROOT

SOURCES += \
    main.cpp \
    widget.cpp \
    $$APP_ROOT/contracts/detection_mode.cpp \
    $$APP_ROOT/system_support/license/license_codec.cpp

HEADERS += \
    widget.h \
    $$APP_ROOT/contracts/detection_mode.h \
    $$APP_ROOT/system_support/license/license_codec.h

FORMS += \
    widget.ui
