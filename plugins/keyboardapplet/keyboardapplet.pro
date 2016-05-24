TEMPLATE        = lib
CONFIG         += plugin

QT += widgets x11extras xml


DESTDIR         = ../

INCLUDEPATH    += ../../lib/


LIBS += -L../../ -lhdepanel

HEADERS += \
    ../../lib/applet.h \
    keyboardapplet.h \
    keyboardlayoutdialog.h \
    keyboard.h



SOURCES += \
    keyboardapplet.cpp \
    keyboardlayoutdialog.cpp \
    keyboard.cpp



#LIBS += -lX11 -lGL -lXdamage -lXcomposite

FORMS += \
    keyboardlayoutdialog.ui
