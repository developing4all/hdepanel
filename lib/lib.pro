######################################################################
# HDE Panel Library Build
######################################################################

TEMPLATE = lib
TARGET = hdepanel
DESTDIR = ../

INCLUDEPATH += .

QT += widgets gui-private dbus
lessThan(QT_MAJOR_VERSION, 6) {
    QT += x11extras
}

# LayerShellQt
greaterThan(QT_MAJOR_VERSION, 5) {
    exists(/usr/include/LayerShellQt/shell.h) {
        DEFINES += HDE_HAVE_LAYERSHELLQT
        LIBS += -lLayerShellQtInterface
        message("[Qt6] Using external LayerShellQtInterface")
    } else {
        message("[Qt6] External LayerShellQt not found; continuing without LayerShellQt")
    }
} else {
    # Qt5: LayerShellQt is not supported due to plugin loading issues
    # Users should use Qt6 for proper Wayland layer-shell support
    message("==================================================================================")
    message("[Qt5] LayerShellQt requires Qt6 for proper Wayland support.")
    message("[Qt5] Please rebuild with Qt6 for optimal Wayland experience.")
    message("[Qt5] Building without LayerShellQt - will use fallback positioning.")
    message("==================================================================================")
}

greaterThan(QT_MAJOR_VERSION, 5) {
    QT += waylandclient
}

VERSION = 1.9.0

######################################################################
# Wayland support
######################################################################
lessThan(QT_MAJOR_VERSION, 6) {
    CONFIG += link_pkgconfig
    packagesExist(wayland-client wayland-cursor) {
        PKGCONFIG += wayland-client wayland-cursor
        DEFINES += HDE_HAVE_WAYLAND
    } else {
        message("Wayland client libs not found via pkg-config; building without layer-shell")
        DEFINES += HDE_NO_WAYLAND
    }
} else {
    # Qt6: direct Wayland linking
    DEFINES += HDE_HAVE_WAYLAND
    LIBS += -lwayland-client -lwayland-cursor
}


######################################################################
# Protocols
######################################################################
contains(DEFINES, HDE_HAVE_WAYLAND) {
    # wlroots layer-shell
    WAYLAND_PROTOCOL = $$PWD/protocols/wlr-layer-shell-unstable-v1.xml
    XDG_SHELL_PROTOCOL = /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

    system(wayland-scanner client-header $$WAYLAND_PROTOCOL $$OUT_PWD/wlr-layer-shell-unstable-v1-client-protocol.h)
    system(wayland-scanner private-code $$WAYLAND_PROTOCOL $$OUT_PWD/wlr-layer-shell-unstable-v1-protocol.c)
    system(wayland-scanner client-header $$XDG_SHELL_PROTOCOL $$OUT_PWD/xdg-shell-client-protocol.h)
    system(wayland-scanner private-code $$XDG_SHELL_PROTOCOL $$OUT_PWD/xdg-shell-protocol.c)

    SOURCES += $$OUT_PWD/wlr-layer-shell-unstable-v1-protocol.c \
               $$OUT_PWD/xdg-shell-protocol.c
    HEADERS += $$OUT_PWD/wlr-layer-shell-unstable-v1-client-protocol.h \
               $$OUT_PWD/xdg-shell-client-protocol.h
}

######################################################################
# Headers and sources
######################################################################
HEADERS += animationutils.h \
           applet.h \
           desktopapplications.h \
           desktopdatastore.h \
           dpisupport.h \
           iconloader.h \
           unifiediconservice.h \
           panelapplication.h \
           panelwindow.h \
           textgraphicsitem.h \
           x11support.h \
           waylandsupport.h \
           waylandlayershell.h \
           layershellqtintegration.h \
           panelsettings.h \
           settings.h \
           hpopupmenu.h \
           appletslistdialog.h \
           waylandwindow.h \
           windowmanagers/windowmanager.h \
           windowmanagers/gnomewindowmanager.h \
           windowmanagers/hyprlandwindowmanager.h \
           windowmanagers/wayfirewindowmanager.h

FORMS += panelsettings.ui \
         appletslistdialog.ui

SOURCES += applet.cpp \
           desktopapplications.cpp \
           desktopdatastore.cpp \
           dpisupport.cpp \
           iconloader.cpp \
           unifiediconservice.cpp \
           panelapplication.cpp \
           panelwindow.cpp \
           textgraphicsitem.cpp \
           x11support.cpp \
           waylandsupport.cpp \
           waylandlayershell.cpp \
           layershellqtintegration.cpp \
           panelsettings.cpp \
           settings.cpp \
           hpopupmenu.cpp \
           appletslistdialog.cpp \
           windowmanagers/windowmanager.cpp \
           windowmanagers/gnomewindowmanager.cpp \
           windowmanagers/hyprlandwindowmanager.cpp \
           windowmanagers/wayfirewindowmanager.cpp

######################################################################
# Qxt for global shortcuts (Qt5)
######################################################################
lessThan(QT_MAJOR_VERSION, 6) {
    HEADERS += 3rdparty/qxt/qxtglobal.h \
               3rdparty/qxt/qxtglobalshortcut.h \
               3rdparty/qxt/qxtglobalshortcut_p.h

    SOURCES += 3rdparty/qxt/qxtglobalshortcut.cpp \
               3rdparty/qxt/qxtglobalshortcut_x11.cpp
}

######################################################################
# Libraries
######################################################################
LIBS += -lGL
lessThan(QT_MAJOR_VERSION, 6) {
    LIBS += -lX11 -lXdamage -lXcomposite
} else {
    LIBS += -lX11 -lXdamage -lXcomposite -lXrender -lXfixes
}
