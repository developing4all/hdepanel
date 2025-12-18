TEMPLATE        = lib
CONFIG         += plugin

QT += widgets


DESTDIR         = ../

INCLUDEPATH    += ../../lib/


##HEADERS += \
##    maintopplugin.h

##SOURCES += \
##    maintopplugin.cpp

#HEADERS += \
#    brainplugin.h

#SOURCES += \
#    brainplugin.cpp

HEADERS += \
    testapplet.h \
    ../../lib/applet.h


SOURCES += \
    testapplet.cpp

TRANSLATIONS += \
    translations/testapplet_ar.ts \
    translations/testapplet_nl.ts \
    translations/testapplet_en.ts

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

OTHER_FILES += \
    TestApplet.json


LIBS += -lX11 -lGL -lXdamage -lXcomposite

LIBS += -L../../ -lhdepanel
