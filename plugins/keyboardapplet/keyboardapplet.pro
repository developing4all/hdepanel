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



SOURCES += \
    keyboardapplet.cpp \
    keyboardlayoutdialog.cpp \
    keyboard.cpp


#LIBS += -lX11 -lGL -lXdamage -lXcomposite
#LIBS += -lQxtCore -lQxtGui

FORMS += \
    keyboardlayoutdialog.ui
