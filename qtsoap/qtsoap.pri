exists(config.pri):infile(config.pri, SOLUTIONS_LIBRARY, yes): CONFIG += qtsoap-uselib
TEMPLATE += fakelib
QTSOAP_LIBNAME = $$qtLibraryTarget(QtSolutions_SOAP-head)
TEMPLATE -= fakelib
QTSOAP_LIBDIR = $$PWD/lib
unix:qtsoap-uselib:!qtsoap-buildlib:QMAKE_RPATHDIR += $$QTSOAP_LIBDIR

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
QT += xml network

qtsoap-uselib:!qtsoap-buildlib {
    LIBS += -L$$QTSOAP_LIBDIR -l$$QTSOAP_LIBNAME
} else {
    SOURCES += qtsoap/qtsoap.cpp
    HEADERS += qtsoap/qtsoap.h
}

win32 {
    contains(TEMPLATE, lib):contains(CONFIG, shared):DEFINES += QT_QTSOAP_EXPORT
    else:qtsoap-uselib:DEFINES += QT_QTSOAP_IMPORT
}
