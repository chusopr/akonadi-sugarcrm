TEMPLATE = app
INCLUDEPATH += .
CONFIG += console

include(qtsoap/qtsoap.pri)

HEADERS += sugarsoap.h
SOURCES += main.cpp sugarsoap.cpp
