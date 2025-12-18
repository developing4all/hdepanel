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
    sni.h \
    dbusmenu.h


SOURCES += \
    trayapplet.cpp \
    sni.cpp \
    dbusmenu.cpp

TRANSLATIONS += \
    translations/trayapplet_ar.ts \
    translations/trayapplet_nl.ts \
    translations/trayapplet_en.ts

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
