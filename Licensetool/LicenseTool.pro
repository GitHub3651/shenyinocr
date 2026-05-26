QT += core gui widgets

CONFIG += c++11
CONFIG -= app_bundle

TEMPLATE = app
TARGET = LicenseTool

DESTDIR = $$OUT_PWD

SOURCES += \
    main.cpp \
    widget.cpp

HEADERS += \
    widget.h

FORMS += \
    widget.ui
