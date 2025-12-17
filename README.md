# HDEPanel

A lightweight, modern desktop panel for Linux with Wayland and X11 support.

Built with Qt5/Qt6, HDEPanel is a fork of QtPanel with extensive improvements including multi-language support, Wayland compatibility, and modern desktop integration.

## Features

- ✨ **Very lightweight** - Minimal resource usage
- 🎨 **Beautiful UI** - Clean, modern appearance
- 🖥️ **Multi-monitor support** - Configure panels per screen
- 🔌 **Plugin architecture** - Extensible applet system
- 🌍 **Multi-language** - English, Dutch, Arabic (83% coverage)
- 🪟 **Wayland + X11** - Dual protocol support
- ⚙️ **Per-panel configuration** - Independent panel settings
- 📱 **XDG compliant** - Follows freedesktop.org standards
- 🔧 **Qt5 & Qt6 compatible** - Works with both Qt versions

## Applets

- **Start Menu** - Application launcher with favorites, recently used, search
- **Task Bar** - Window management with drag-and-drop, grouping
- **System Tray** - Notification area for background apps
- **Clock** - Time display with calendar popup
- **Keyboard Layout** - Switch keyboard layouts
- **Applications Menu** - Classic applications menu by category

## Build Dependencies

HDEPanel supports both Qt5 and Qt6. Choose the version available on your system.

**Note**: Translation tools (`lrelease`, `lupdate`) are required for building. These are provided by:
- Qt6: `qt6-l10n-tools` (provides `/usr/lib/qt6/bin/lrelease`)
- Qt5: `qttools5-dev-tools` (provides `/usr/lib/qt5/bin/lrelease`)

### Ubuntu/Debian

**Qt6** (recommended for Ubuntu 22.04+):
```bash
sudo apt install \
    build-essential \
    qt6-base-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    qt6-l10n-tools \
    qt6-wayland \
    libqt6waylandclient6 \
    libgl1-mesa-dev \
    libx11-dev \
    libxdamage-dev \
    libxcomposite-dev \
    libxrender-dev \
    libxfixes-dev \
    libwayland-dev \
    libxkbcommon-dev \
    wayland-protocols
```

**Qt5** (for Ubuntu 20.04 or older):
```bash
sudo apt install \
    build-essential \
    qtbase5-dev \
    qttools5-dev \
    qttools5-dev-tools \
    libqt5x11extras5-dev \
    libgl1-mesa-dev \
    libx11-dev \
    libxdamage-dev \
    libxcomposite-dev \
    libxrender-dev \
    libxfixes-dev \
    libwayland-dev \
    libxkbcommon-dev \
    wayland-protocols
```

### Fedora/RHEL

**Qt6**:
```bash
sudo dnf install \
    gcc-c++ \
    make \
    qt6-qtbase-devel \
    qt6-qttools-devel \
    qt6-linguist \
    qt6-qtwayland-devel \
    mesa-libGL-devel \
    libX11-devel \
    libXdamage-devel \
    libXcomposite-devel \
    libXrender-devel \
    libXfixes-devel \
    wayland-devel \
    libxkbcommon-devel \
    wayland-protocols-devel
```
*Note*: `qt6-linguist` provides `/usr/lib64/qt6/bin/lrelease`

**Qt5**:
```bash
sudo dnf install \
    gcc-c++ \
    make \
    qt5-qtbase-devel \
    qt5-qttools-devel \
    qt5-linguist \
    qt5-qtx11extras-devel \
    mesa-libGL-devel \
    libX11-devel \
    libXdamage-devel \
    libXcomposite-devel \
    libXrender-devel \
    libXfixes-devel \
    wayland-devel \
    libxkbcommon-devel \
    wayland-protocols-devel
```
*Note*: `qt5-linguist` provides `/usr/lib64/qt5/bin/lrelease`

### Arch Linux

**Qt6**:
```bash
sudo pacman -S \
    base-devel \
    qt6-base \
    qt6-tools \
    qt6-wayland \
    mesa \
    libx11 \
    libxdamage \
    libxcomposite \
    libxrender \
    libxfixes \
    wayland \
    libxkbcommon \
    wayland-protocols
```
*Note*: `qt6-tools` provides `/usr/lib/qt6/bin/lrelease`

**Qt5**:
```bash
sudo pacman -S \
    base-devel \
    qt5-base \
    qt5-tools \
    qt5-x11extras \
    mesa \
    libx11 \
    libxdamage \
    libxcomposite \
    libxrender \
    libxfixes \
    wayland \
    libxkbcommon \
    wayland-protocols
```
*Note*: `qt5-tools` provides `/usr/lib/qt5/bin/lrelease`

## Building

```bash
# Clone the repository
git clone https://github.com/yourusername/hdepanel.git
cd hdepanel

# Build with Qt6
qmake6
make

# OR build with Qt5
qmake
make
```

**Note**: The project automatically detects Qt version. Translations compile automatically during build.

### Troubleshooting Build Issues

**Error**: `lrelease: could not exec '/usr/lib/qt5/bin/lrelease': No such file or directory`

This happens when `qtchooser` is misconfigured. Solutions:
1. Install the missing Qt tools package:
   - Ubuntu/Debian: `sudo apt install qt6-l10n-tools` (Qt6) or `qttools5-dev-tools` (Qt5)
2. Or configure `qtchooser`:
   ```bash
   # Set Qt6 as default
   sudo update-alternatives --install /usr/bin/qmake qmake /usr/lib/qt6/bin/qmake 60
   ```
3. Or use the direct path:
   ```bash
   # Force Qt6
   /usr/lib/qt6/bin/qmake
   make
   ```

### Quick Build Commands

From project root:
```bash
make              # Build everything (compiles translations automatically)
make clean        # Clean build artifacts
make install      # Install to system (default: /usr/local)
make help         # Show all available targets
```

See [BUILD.md](BUILD.md) for detailed build instructions.

## Installation

### System-wide Installation

```bash
sudo make install
```

Or with custom prefix:
```bash
sudo make install PREFIX=/usr
```

### Package Installation

```bash
# Build Debian package
dpkg-buildpackage -b -uc -us

# Install
sudo dpkg -i ../hdepanel_*.deb
```

## Running

### Development Build

```bash
# From build directory
export LD_LIBRARY_PATH=./:$LD_LIBRARY_PATH
./hdepanel
```

Or use the launch script:
```bash
./hdepanel-launch.sh
```

### Test Different Languages

```bash
./test-languages.sh en    # English
./test-languages.sh nl    # Dutch
./test-languages.sh ar    # Arabic
```

### Installed Version

```bash
hdepanel
```

## Translation

HDEPanel supports multiple languages:
- English (100%)
- Dutch (83%)
- Arabic (83%)

To update translations:
```bash
# Extract translatable strings (Qt6)
lupdate -recursive . -ts translations/hdepanel_*.ts

# OR with Qt5
lupdate-qt5 -recursive . -ts translations/hdepanel_*.ts

# Edit translations (use Qt Linguist or edit .ts files)
linguist translations/hdepanel_nl.ts

# Rebuild (translations compile automatically)
make
```

See [translations/README.md](translations/README.md) for details.

## Configuration

Panel settings are stored in `~/.config/hde/panel.conf`.

Right-click the panel to access:
- Configure Panel
- Add/Remove Applets
- Panel Position & Size
- Display Settings

## Platform Support

### Wayland
- ✅ GNOME Wayland
- ✅ KDE Plasma Wayland  
- ✅ Sway
- ⚠️ Uses Xwayland fallback for proper positioning on GNOME

### X11
- ✅ Full support on all X11 desktops
- ✅ EWMH compliant
- ✅ Multi-monitor aware

See [WAYLAND_COMPATIBILITY.md](WAYLAND_COMPATIBILITY.md) for details.

## Development

### Project Structure

```
hdepanel/
├── lib/              # Core panel library
├── plugins/          # Applet plugins
│   ├── startapplet/
│   ├── taskbarapplet/
│   ├── trayapplet/
│   ├── clockapplet/
│   └── keyboardapplet/
├── app/              # Main application
├── translations/     # i18n translation files
└── debian/           # Debian packaging
```

### Adding Translations

1. Wrap user-visible strings in `tr()`:
   ```cpp
   menu.addAction(tr("My Action"));
   ```

2. Extract and compile:
   ```bash
   # Qt6
   lupdate -recursive . -ts translations/*.ts
   
   # Qt5
   lupdate-qt5 -recursive . -ts translations/*.ts
   
   # Build (works with both Qt5/Qt6)
   make
   ```

### Qt Version Compatibility

HDEPanel is designed to work with both Qt5 and Qt6:
- **Qt5**: Tested on Qt 5.12+ (Ubuntu 20.04, Debian 10+)
- **Qt6**: Tested on Qt 6.2+ (Ubuntu 22.04+, Fedora 35+)

The build system automatically detects the Qt version and adjusts accordingly.

## Contributing

Contributions are welcome! Please:
- Follow Qt coding style
- Add translations for new user-visible strings
- Test on both Wayland and X11
- Update documentation

## License

LGPL 3.0+ - See [LICENSE](LICENSE) file

## Credits

Based on [QtPanel](https://github.com/qtpanel/qtpanel) by Leslie Zhai and contributors.

Extended and maintained by Haydar Alkaduhimi.

## Links

- **Source**: https://github.com/yourusername/hdepanel
- **Issues**: https://github.com/yourusername/hdepanel/issues
- **Wiki**: https://github.com/yourusername/hdepanel/wiki




