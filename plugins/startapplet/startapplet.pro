TEMPLATE        = lib
CONFIG         += plugin

QT += widgets


DESTDIR         = ../

INCLUDEPATH    += ../../lib/




#LIBS += -lX11 -lGL -lXdamage -lXcomposite

LIBS += -L../../ -lhdepanel

HEADERS += \
    ../../lib/applet.h \
    startapplet.h\
    startwindow.h \
    hitemlist.h


SOURCES += \
    startapplet.cpp\
    startwindow.cpp \
    hitemlist.cpp

TRANSLATIONS += \
    translations/startapplet_ar.ts \
    translations/startapplet_nl.ts \
    translations/startapplet_en.ts

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

FORMS    += \
    startwindow.ui


#LIBS += -lX11 -lGL -lXdamage -lXcomposite
