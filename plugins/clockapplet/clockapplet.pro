TEMPLATE        = lib
CONFIG         += plugin

QT += widgets

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    clockapplet.h \
    ../../lib/applet.h \
    calendar.h


SOURCES += \
    clockapplet.cpp \
    calendar.cpp



LIBS += -L../../ -lhdepanel

FORMS += \
    calendar.ui
