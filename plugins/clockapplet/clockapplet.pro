TEMPLATE        = lib
CONFIG         += plugin

QT += widgets

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    clockapplet.h \
    clockconfigurationdialog.h \
    ../../lib/applet.h \
    calendar.h


SOURCES += \
    clockapplet.cpp \
    clockconfigurationdialog.cpp \
    calendar.cpp



LIBS += -L../../ -lhdepanel

FORMS += \
    calendar.ui \
    clockconfigurationdialog.ui
