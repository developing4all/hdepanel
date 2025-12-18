TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS = lib \
	plugins \
	app

# Translation files
TRANSLATIONS += translations/hdepanel_en.ts \
                translations/hdepanel_nl.ts \
                translations/hdepanel_ar.ts

# Compile translations automatically during build
# Find lrelease tool
isEmpty(QMAKE_LRELEASE) {
    win32: QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
    else: QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease
    unix {
        !exists($$QMAKE_LRELEASE) { QMAKE_LRELEASE = lrelease }
    }
}

# Custom target to compile translations
lrelease.commands = \
    @echo "Compiling translations..." && \
    $$QMAKE_LRELEASE $$PWD/translations/hdepanel_en.ts -qm $$PWD/translations/hdepanel_en.qm && \
    $$QMAKE_LRELEASE $$PWD/translations/hdepanel_nl.ts -qm $$PWD/translations/hdepanel_nl.qm && \
    $$QMAKE_LRELEASE $$PWD/translations/hdepanel_ar.ts -qm $$PWD/translations/hdepanel_ar.qm
lrelease.target = lrelease
lrelease.depends = FORCE
QMAKE_EXTRA_TARGETS += lrelease

# Hook into make_first to compile translations automatically
make_first.depends = lrelease
QMAKE_EXTRA_TARGETS += make_first

# Install translations
translations.path = $$PREFIX/share/hdepanel/translations
translations.files = translations/*.qm
INSTALLS += translations

# Install applet translations
applet_translations.path = $$PREFIX/lib/hde/panel/plugins
applet_translations.files = plugins/*.qm
INSTALLS += applet_translations

