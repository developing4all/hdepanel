TEMPLATE        = lib
CONFIG         += plugin

QT += widgets
lessThan(QT_MAJOR_VERSION, 6) {
    QT += x11extras
}

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    taskbarapplet.h \
    taskbaritem.h \
    client.h \
    waylandclient.h \
    taskbarappletplugin.h \
    ../../lib/applet.h \
    taskbarconfigurationdialog.h


SOURCES += \
    taskbarapplet.cpp \
    taskbaritem.cpp \
    client.cpp \
    waylandclient.cpp \
    taskbarappletplugin.cpp \
    taskbarconfigurationdialog.cpp



LIBS += -L../../ -lhdepanel

FORMS += \
    taskbarconfigurationdialog.ui
