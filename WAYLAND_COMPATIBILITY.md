# Wayland Compatibility Guide

## Overview

hdepanel now supports **both X11 and Wayland**, with automatic detection and appropriate configuration for different display server environments.

## Supported Environments

### ✅ Fully Supported (Complete Functionality)

| Environment   | Panel Positioning | Space Reservation | Notes                     |
|---------------|-------------------|-------------------|---------------------------|
| **X11 (all)** | ✅ Perfect        | ✅ Perfect        | Recommended for all users |
| **Sway**      | ✅ Perfect        | ✅ Perfect        | Uses wlr-layer-shell      |
| **Hyprland**  | ✅ Perfect        | ✅ Perfect        | Uses wlr-layer-shell      |
| **River**     | ✅ Perfect        | ✅ Perfect        | Uses wlr-layer-shell      |
| **Wayfire**   | ✅ Perfect        | ✅ Perfect        | Uses wlr-layer-shell      |

### ⚠️ Partial Support (Limited Functionality)

| Environment           | Panel Positioning | Space Reservation | Notes                             |
|-----------------------|-------------------|-------------------|-----------------------------------|
| **GNOME on Wayland**  | ✅ Works          | ❌ No             | Use "GNOME on Xorg" instead       |
| **KDE on Wayland**    | ✅ Works          | ❌ No             | Use "Plasma on X11" instead       |
| **Weston**            | ✅ Works          | ❌ No             | Use X11 or wlr-based compositor   |

## Quick Start

### Running the Panel

Simply use the launcher script which auto-detects your environment:

```bash
./hdepanel-launch.sh
```

The launcher will:
- Detect if you're running X11 or Wayland
- Identify your compositor
- Show appropriate warnings/recommendations
- Configure the panel appropriately

### Manual Launch

For advanced users:

```bash
# Auto-detect (recommended)
LD_LIBRARY_PATH=. ./hdepanel

# Force X11 mode (works everywhere via XWayland)
QT_QPA_PLATFORM=xcb LD_LIBRARY_PATH=. ./hdepanel

# Force Wayland mode
QT_QPA_PLATFORM=wayland LD_LIBRARY_PATH=. ./hdepanel
```

## Understanding the Limitations

### Why doesn't it work perfectly on GNOME/KDE Wayland?

**Wayland has no universal panel protocol.** Different compositors use incompatible protocols:

- **wlr-based compositors** (Sway, Hyprland, etc.) → `wlr-layer-shell-unstable-v1` ✅ (implemented)
- **GNOME Shell** → Private/undocumented protocols ❌
- **KDE Plasma** → KDE-specific protocols ❌ (not yet implemented)
- **Weston** → Weston-specific protocols ❌

This is a **fundamental Wayland limitation**, not a bug. Even professional projects like Waybar and yambar only support wlr-based compositors.

### What does "Partial Support" mean?

On GNOME/KDE Wayland:
- ✅ Panel positions correctly (top/bottom)
- ✅ Panel stays on top
- ✅ All applets work
- ❌ Cannot reserve space (windows may overlap panel)
- ❌ Cannot prevent fullscreen windows from covering panel

## Solutions for Each Environment

### GNOME Users

**Recommended:** Use X11 session
1. Log out
2. At login screen, click the gear icon ⚙️
3. Select "GNOME on Xorg" or "Ubuntu on Xorg"
4. Login and run `./hdepanel-launch.sh`

**Alternative:** Accept limited functionality on Wayland
- Panel works but doesn't reserve space
- Manual window positioning needed

### KDE Plasma Users

**Recommended:** Use X11 session
1. Log out
2. Select "Plasma (X11)" at login screen
3. Login and run `./hdepanel-launch.sh`

### Tiling WM Users

**Perfect support!** Just run the panel:
```bash
# Add to your compositor config (Sway example)
exec ~/Projects/Qt/hdepanel/hdepanel-launch.sh
```

Works perfectly on:
- Sway
- Hyprland
- River
- Wayfire
- labwc
- Any wlroots-based compositor

## Technical Details

### Protocols Implemented

#### X11 Support
- `_NET_WM_STRUT` and `_NET_WM_STRUT_PARTIAL` for space reservation
- `_NET_WM_WINDOW_TYPE_DOCK` for panel window type
- Works on all X11 window managers and through XWayland

#### Wayland Support
- `wlr-layer-shell-unstable-v1` for wlr-based compositors
- Automatic compositor detection
- Graceful fallback when protocols unavailable

### Compositor Detection

The panel automatically detects your compositor using:
- `XDG_CURRENT_DESKTOP` environment variable
- `XDG_SESSION_DESKTOP` environment variable  
- `WAYLAND_DISPLAY` patterns
- Display server type (X11 vs Wayland)

## Troubleshooting

### Panel doesn't reserve space on Wayland

**Expected behavior on GNOME/KDE Wayland.** Solutions:
1. Switch to X11 session (best)
2. Switch to wlr-based compositor
3. Accept the limitation and manually manage windows

### Panel appears in wrong position

Check your configuration:
```bash
cat ~/.config/developing4all/hde/panel.conf
```

Look for `verticalPosition=Top` or `verticalPosition=Bottom`

### "Layer-shell interface not available" message

This means your compositor doesn't support wlr-layer-shell. This is normal on GNOME, KDE, and Weston. The panel will still work but won't reserve space.

## Development

### Adding Support for Other Protocols

To add support for additional Wayland protocols (e.g., KDE Plasma Shell):

1. Add protocol XML to `lib/protocols/`
2. Update `lib.pro` to generate bindings
3. Implement initialization in `PanelWindow::initWaylandLayerShell()`
4. Add compositor detection in `PanelWindow::detectWaylandCompositor()`

Contributions welcome!

## FAQ

**Q: Why not just support everything?**  
A: Different compositors use incompatible protocols. Implementing all of them would require months of work and ongoing maintenance.

**Q: Will Qt6 fix this?**  
A: No. Qt6 has better Wayland support, but the protocol incompatibility is a compositor issue, not a Qt issue.

**Q: Why does Waybar work on Sway but not GNOME?**  
A: Waybar only supports wlr-layer-shell (like this panel). It doesn't work on GNOME Wayland either.

**Q: Can I use this panel on GNOME Wayland?**  
A: Yes, but windows won't avoid it. For full functionality, use "GNOME on Xorg" session.

**Q: Which is better: X11 or Wayland?**  
A: For this panel, X11 provides the most consistent experience across all desktop environments.

## Summary

- ✅ **X11 → Works perfectly everywhere**
- ✅ **wlr compositors → Works perfectly with native Wayland**
- ⚠️ **GNOME/KDE Wayland → Works but limited, use X11 session instead**

For questions or issues, please check the main README or open an issue on GitHub.

