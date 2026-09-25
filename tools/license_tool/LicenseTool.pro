QT += core gui widgets

CONFIG += c++11
CONFIG -= app_bundle

msvc {
    QMAKE_CXXFLAGS += /utf-8
}

TEMPLATE = app
TARGET = LicenseTool

DESTDIR = $$OUT_PWD

SOURCES += \
    activation_protocol.cpp \
    main.cpp \
    widget.cpp

HEADERS += \
    activation_protocol.h \
    widget.h

FORMS += \
    widget.ui
