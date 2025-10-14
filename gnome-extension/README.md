# HDEPanel Window List GNOME Shell Extension

This GNOME Shell extension provides window list information to HDEPanel via a D-Bus interface. It's a **safe** alternative to using GNOME Shell's unsafe eval mode.

## Why Use This Extension?

### Without Extension (Unsafe Mode):
- Requires enabling `org.gnome.Shell.Eval` which allows **arbitrary JavaScript execution**
- Security risk - any application can execute code in GNOME Shell
- Disabled by default on many distributions
- Command to enable: `gsettings set org.gnome.shell development-tools true`

### With Extension (Safe):
- ✅ No unsafe mode required
- ✅ Proper D-Bus API
- ✅ Sandboxed and secure
- ✅ Easy to install and uninstall
- ✅ Works on all GNOME versions (45, 46, 47+)

## Installation

### Quick Install

```bash
cd gnome-extension
chmod +x install.sh
./install.sh
```

### Manual Install

```bash
# Copy extension to local extensions directory
mkdir -p ~/.local/share/gnome-shell/extensions
cp -r hdepanel-window-list@hdepanel ~/.local/share/gnome-shell/extensions/

# Enable the extension
gnome-extensions enable hdepanel-window-list@hdepanel
```

### Restart GNOME Shell

**On X11:**
```bash
# Press Alt+F2, type 'r', press Enter
```

**On Wayland:**
```bash
# Log out and log back in
```

## Verify Installation

Check if the extension is enabled:
```bash
gnome-extensions list --enabled | grep hdepanel
```

Test the D-Bus interface:
```bash
gdbus call --session \
  --dest org.gnome.Shell \
  --object-path /org/hdepanel/WindowList \
  --method org.hdepanel.WindowList.GetWindows
```

You should see a JSON array of window information.

## Using with HDEPanel

HDEPanel will automatically detect and use this extension if available. If the extension is not found, it will fall back to unsafe mode (if enabled).

Check HDEPanel logs to see which method is being used:
```bash
# With extension (preferred)
WaylandSupport: Using HDEPanel extension for window list

# Without extension (fallback)
WaylandSupport: HDEPanel extension not found, falling back to unsafe mode
WaylandSupport: Install the extension from gnome-extension/hdepanel-window-list@hdepanel
```

## D-Bus Interface

### Service
- **Bus**: Session Bus
- **Name**: `org.gnome.Shell`
- **Object Path**: `/org/hdepanel/WindowList`
- **Interface**: `org.hdepanel.WindowList`

### Methods

#### GetWindows
Returns a JSON string containing an array of window objects.

**Signature:** `() -> s`

**Example:**
```bash
gdbus call --session \
  --dest org.gnome.Shell \
  --object-path /org/hdepanel/WindowList \
  --method org.hdepanel.WindowList.GetWindows
```

**Response Format:**
```json
[
  {
    "id": 12345,
    "title": "Firefox",
    "app_id": "firefox",
    "wm_class": "firefox",
    "window_type": 0,
    "visible": true,
    "focused": false,
    "is_wayland": true,
    "is_panel": false,
    "is_system_service": false,
    "minimized": false,
    "maximized": false,
    "on_all_workspaces": false
  }
]
```

### Signals

#### WindowsChanged
Emitted when the window list changes (new window, closed window, etc.).

**Signature:** `()`

**Monitor signal:**
```bash
gdbus monitor --session --dest org.gnome.Shell --object-path /org/hdepanel/WindowList
```

## Troubleshooting

### Extension not showing up
```bash
# Check extension is in the right place
ls -la ~/.local/share/gnome-shell/extensions/hdepanel-window-list@hdepanel/

# Check for errors in GNOME Shell logs
journalctl -f /usr/bin/gnome-shell
```

### Extension installed but not working
```bash
# Check if extension is enabled
gnome-extensions list --enabled

# Enable manually
gnome-extensions enable hdepanel-window-list@hdepanel

# Check extension info
gnome-extensions info hdepanel-window-list@hdepanel
```

### D-Bus interface not available
```bash
# Restart GNOME Shell (X11 only)
Alt+F2 -> type 'r' -> Enter

# Or log out and back in (Wayland)
```

## Uninstallation

```bash
# Disable extension
gnome-extensions disable hdepanel-window-list@hdepanel

# Remove extension
rm -rf ~/.local/share/gnome-shell/extensions/hdepanel-window-list@hdepanel

# Restart GNOME Shell (X11) or log out/in (Wayland)
```

## Development

### Testing Changes

After modifying the extension:

```bash
# Reload the extension
gnome-extensions disable hdepanel-window-list@hdepanel
gnome-extensions enable hdepanel-window-list@hdepanel

# Or restart GNOME Shell
# X11: Alt+F2 -> 'r' -> Enter
# Wayland: Log out and back in
```

### Debugging

View extension logs:
```bash
journalctl -f -o cat /usr/bin/gnome-shell | grep -i hdepanel
```

Or use Looking Glass (Alt+F2, type `lg`):
```javascript
// List windows
global.display.get_tab_list(0, null).map(w => w.get_title())
```

## Compatibility

- **GNOME Shell**: 45, 46, 47+
- **Distributions**: Ubuntu 24.04+, Fedora 40+, Arch Linux (current)

## License

GPL 3.0 - Same as HDEPanel

## Support

For issues, please open a ticket at:
https://github.com/yourusername/hdepanel/issues

