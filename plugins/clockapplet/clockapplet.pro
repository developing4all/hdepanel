TEMPLATE        = lib
CONFIG         += plugin

QT += widgets

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    clockapplet.h \
    ../../lib/applet.h


SOURCES += \
    clockapplet.cpp



LIBS += -L../../ -lhdepanel
