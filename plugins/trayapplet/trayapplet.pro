TEMPLATE        = lib
CONFIG         += plugin

QT += widgets x11extras

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    trayapplet.h \
    ../../lib/applet.h


SOURCES += \
    trayapplet.cpp



LIBS += -L../../ -lhdepanel
