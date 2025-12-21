TEMPLATE        = lib
CONFIG         += plugin

QT += widgets dbus

DESTDIR         = ../

INCLUDEPATH    += ../../lib/


HEADERS += \
    networkmanagerapplet.h \
    ../../lib/applet.h


SOURCES += \
    networkmanagerapplet.cpp

TRANSLATIONS += \
    translations/networkmanagerapplet_ar.ts \
    translations/networkmanagerapplet_nl.ts \
    translations/networkmanagerapplet_en.ts

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

