TEMPLATE = app
INCLUDEPATH += .
CONFIG += console

include(../src/qtsoap.pri)

HEADERS += sugarsoap.h
SOURCES += main.cpp sugarsoap.cpp
