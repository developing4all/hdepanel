# Wrapper Makefile to automatically compile translations
#
# This Makefile ensures translations are always compiled before building

.PHONY: all clean distclean install lrelease qmake

all: lrelease
	@cd build && $(MAKE)

lrelease:
	@cd build && $(MAKE) lrelease

clean:
	@cd build 2>/dev/null && $(MAKE) clean || true

distclean:
	@cd build 2>/dev/null && $(MAKE) distclean || true
	@rm -rf build

install:
	@cd build && $(MAKE) install

qmake:
	@mkdir -p build
	@cd build && qmake6 ../hdepanel.pro

help:
	@echo "HDEPanel Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Compile translations and build project (default)"
	@echo "  lrelease   - Compile translation files only"
	@echo "  clean      - Clean build artifacts"
	@echo "  distclean  - Remove all generated files including build/"
	@echo "  install    - Install to system"
	@echo "  qmake      - Regenerate Makefiles"
	@echo ""
	@echo "Translations are automatically compiled before building."

