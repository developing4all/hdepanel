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

TRANSLATIONS += \
    translations/clockapplet_ar.ts \
    translations/clockapplet_nl.ts \
    translations/clockapplet_en.ts

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
    calendar.ui \
    clockconfigurationdialog.ui
