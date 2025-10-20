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
    keyboard.h



SOURCES += \
    keyboardapplet.cpp \
    keyboardlayoutdialog.cpp \
    keyboard.cpp \
    ../../lib/3rdparty/qxt/qxtglobalshortcut.cpp \
    ../../lib/3rdparty/qxt/qxtglobalshortcut_x11.cpp


#LIBS += -lX11 -lGL -lXdamage -lXcomposite
#LIBS += -lQxtCore -lQxtGui

FORMS += \
    keyboardlayoutdialog.ui
