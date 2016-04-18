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

OTHER_FILES += \
    TestApplet.json


LIBS += -lX11 -lGL -lXdamage -lXcomposite

LIBS += -L../../ -lhdepanel
