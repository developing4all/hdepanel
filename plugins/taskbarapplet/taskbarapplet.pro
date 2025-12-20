TEMPLATE        = lib
CONFIG         += plugin

QT += widgets
lessThan(QT_MAJOR_VERSION, 6) {
    QT += x11extras
}

# IMPORTANT: Must match lib build feature flags to avoid ABI mismatch.
# WaylandSupport's class layout changes based on HDE_HAVE_WAYLAND.
DEFINES += HDE_HAVE_WAYLAND

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

TRANSLATIONS += \
    translations/taskbarapplet_ar.ts \
    translations/taskbarapplet_nl.ts \
    translations/taskbarapplet_en.ts

# Compile translations
isEmpty(QMAKE_LRELEASE) {
    win32: QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else: QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
}

qm_files.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
qm_files.input = TRANSLATIONS
qm_files.output = $$DESTDIR/${QMAKE_FILE_BASE}.qm
qm_files.variable_out = PRE_TARGETDEPS
QMAKE_EXTRA_COMPILERS += qm_files



LIBS += -L../../ -lhdepanel

FORMS += \
    taskbarconfigurationdialog.ui
