QT += core gui qml quick quickcontrols2 multimedia network

CONFIG += c++17 release
TARGET = freccia
TEMPLATE = app

HEADERS += \
    src/instance.h \
    src/radio.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/instance.cpp \
    src/radio.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
