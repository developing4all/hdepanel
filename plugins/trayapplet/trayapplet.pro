TEMPLATE        = lib
CONFIG         += plugin

QT += widgets dbus
lessThan(QT_MAJOR_VERSION, 6) {
    QT += x11extras
}

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    trayapplet.h \
    ../../lib/applet.h \
    sni.h


SOURCES += \
    trayapplet.cpp \
    sni.cpp



LIBS += -L../../ -lhdepanel
