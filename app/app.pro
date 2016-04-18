TEMPLATE = app
TARGET = hdepanel
DESTDIR         = ../

INCLUDEPATH += .
INCLUDEPATH    += ../lib

QT += widgets x11extras

SOURCES += main.cpp


LIBS += -lX11 -lGL -lXdamage -lXcomposite -L../ -lhdepanel



