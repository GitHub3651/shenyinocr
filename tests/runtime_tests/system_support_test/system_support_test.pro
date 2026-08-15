QT += core testlib
CONFIG += console testcase c++11
TEMPLATE = app
TARGET = system_support_test

INCLUDEPATH += ../../../app

SOURCES += \
    system_support_test.cpp \
    ../../../app/system_support/license/license_codec.cpp \
    ../../../app/startup/single_instance_guard.cpp

HEADERS += \
    ../../../app/system_support/license/license_codec.h \
    ../../../app/startup/single_instance_guard.h
