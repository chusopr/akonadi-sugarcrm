TEMPLATE = app
INCLUDEPATH += .
CONFIG += console

include(qtsoap/qtsoap.pri)

HEADERS += sugarcrmresource.h sugarsoap.h
SOURCES += main.cpp sugarcrmresource.cpp sugarsoap.cpp
