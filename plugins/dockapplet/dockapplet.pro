TEMPLATE        = lib
CONFIG         += plugin

QT += widgets x11extras

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    dockapplet.h \
    ../../lib/applet.h


SOURCES += \
    dockapplet.cpp



LIBS += -L../../ -lhdepanel
