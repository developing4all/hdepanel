TEMPLATE = app
TARGET = hdepanel
DESTDIR         = ../

INCLUDEPATH += .
INCLUDEPATH    += ../lib

QT += widgets
lessThan(QT_MAJOR_VERSION, 6) {
    QT += x11extras
}

SOURCES += main.cpp


LIBS += -lGL -L../ -lhdepanel
lessThan(QT_MAJOR_VERSION, 6) {
    LIBS += -lX11 -lXdamage -lXcomposite
} else {
    LIBS += -lX11 -lXdamage -lXcomposite -lXrender -lXfixes
}

# Ensure runtime can locate libhdepanel.so.* next to the binary (literal $ORIGIN)
QMAKE_LFLAGS += -Wl,-rpath,'\$$ORIGIN'



