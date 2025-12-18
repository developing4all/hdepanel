TEMPLATE        = lib
CONFIG         += plugin

QT += widgets xml
lessThan(QT_MAJOR_VERSION, 6) {
    QT += x11extras
}

#CONFIG += qxt
#QXT += core widgets


DESTDIR         = ../

INCLUDEPATH    += ../../lib/
INCLUDEPATH    += ../../lib/3rdparty/qxt
#INCLUDEPATH += /usr/include/qxt/QxtCore/

LIBS += -L../../ -lhdepanel
LIBS += -lX11

HEADERS += \
    ../../lib/applet.h \
    keyboardapplet.h \
    keyboardlayoutdialog.h \
    keyboard.h \
    ../../lib/3rdparty/qxt/qxtglobalshortcut.h \
    ../../lib/3rdparty/qxt/qxtglobalshortcut_p.h



SOURCES += \
    keyboardapplet.cpp \
    keyboardlayoutdialog.cpp \
    keyboard.cpp \
    ../../lib/3rdparty/qxt/qxtglobalshortcut.cpp \
    ../../lib/3rdparty/qxt/qxtglobalshortcut_x11.cpp

TRANSLATIONS += \
    translations/keyboardapplet_ar.ts \
    translations/keyboardapplet_nl.ts \
    translations/keyboardapplet_en.ts

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


#LIBS += -lX11 -lGL -lXdamage -lXcomposite
#LIBS += -lQxtCore -lQxtGui

FORMS += \
    keyboardlayoutdialog.ui
