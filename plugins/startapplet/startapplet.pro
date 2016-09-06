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

FORMS    += \
    startwindow.ui


#LIBS += -lX11 -lGL -lXdamage -lXcomposite
