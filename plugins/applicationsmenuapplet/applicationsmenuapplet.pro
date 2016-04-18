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
    applicationsmenuapplet.h \
    ../../lib/applet.h


SOURCES += \
    applicationsmenuapplet.cpp

#OTHER_FILES += \
#    TestApplet.json



LIBS += -L../../ -lhdepanel -lhdepanel
