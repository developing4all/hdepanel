TEMPLATE        = lib
CONFIG         += plugin

QT += widgets x11extras

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    dockapplet.h \
    ../../lib/applet.h \
    dockconfigurationdialog.h


SOURCES += \
    dockapplet.cpp \
    dockconfigurationdialog.cpp



LIBS += -L../../ -lhdepanel

FORMS += \
    dockconfigurationdialog.ui
