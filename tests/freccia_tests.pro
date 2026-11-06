QT += core gui multimedia network testlib
CONFIG += c++17 testcase
TARGET = freccia_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/instance.h ../src/radio.h ../src/theme.h ../src/translator.h
SOURCES += freccia_tests.cpp ../src/instance.cpp ../src/radio.cpp ../src/theme.cpp ../src/translator.cpp
