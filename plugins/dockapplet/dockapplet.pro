TEMPLATE        = lib
CONFIG         += plugin

QT += widgets
lessThan(QT_MAJOR_VERSION, 6) {
    QT += x11extras
}

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    dockapplet.h \
    dockitem.h \
    client.h \
    waylandclient.h \
    dockappletplugin.h \
    ../../lib/applet.h \
    dockconfigurationdialog.h


SOURCES += \
    dockapplet.cpp \
    dockitem.cpp \
    client.cpp \
    waylandclient.cpp \
    dockappletplugin.cpp \
    dockconfigurationdialog.cpp



LIBS += -L../../ -lhdepanel

FORMS += \
    dockconfigurationdialog.ui
