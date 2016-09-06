TEMPLATE        = lib
CONFIG         += plugin

QT += widgets x11extras xml

#CONFIG += qxt
#QXT += core widgets


DESTDIR         = ../

INCLUDEPATH    += ../../lib/
INCLUDEPATH    += ../../lib/3rdparty/qxt
#INCLUDEPATH += /usr/include/qxt/QxtCore/

LIBS += -L../../ -lhdepanel

HEADERS += \
    ../../lib/applet.h \
    keyboardapplet.h \
    keyboardlayoutdialog.h \
    keyboard.h

HEADERS += \
    ../../lib/3rdparty/qxt/qxtglobal.h \
    ../../lib/3rdparty/qxt/qxtglobalshortcut.h \
    ../../lib/3rdparty/qxt/qxtglobalshortcut_p.h \


SOURCES += \
    keyboardapplet.cpp \
    keyboardlayoutdialog.cpp \
    keyboard.cpp

SOURCES += \
    ../../lib/3rdparty/qxt/qxtglobalshortcut.cpp \
    ../../lib/3rdparty/qxt/qxtglobalshortcut_x11.cpp \


#LIBS += -lX11 -lGL -lXdamage -lXcomposite
#LIBS += -lQxtCore -lQxtGui

FORMS += \
    keyboardlayoutdialog.ui
