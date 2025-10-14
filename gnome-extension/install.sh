#!/bin/bash
# Install HDEPanel Window List GNOME Shell Extension

set -e

EXTENSION_UUID="hdepanel-window-list@hdepanel"
EXTENSIONS_DIR="$HOME/.local/share/gnome-shell/extensions"
EXTENSION_PATH="$EXTENSIONS_DIR/$EXTENSION_UUID"

echo "Installing HDEPanel Window List extension..."

# Create extensions directory if it doesn't exist
mkdir -p "$EXTENSIONS_DIR"

# Remove old version if it exists
if [ -d "$EXTENSION_PATH" ]; then
    echo "Removing old version..."
    rm -rf "$EXTENSION_PATH"
fi

# Copy extension files
echo "Copying extension files..."
cp -r "$EXTENSION_UUID" "$EXTENSIONS_DIR/"

# Set proper permissions
chmod 755 "$EXTENSION_PATH"
chmod 644 "$EXTENSION_PATH"/*.js
chmod 644 "$EXTENSION_PATH"/*.json

echo ""
echo "✅ Extension installed successfully!"
echo ""
echo "To enable the extension, run:"
echo "  gnome-extensions enable $EXTENSION_UUID"
echo ""
echo "Or use GNOME Extensions app (install with: sudo apt install gnome-shell-extension-prefs)"
echo ""
echo "After enabling, restart GNOME Shell:"
echo "  - On X11: Press Alt+F2, type 'r', press Enter"
echo "  - On Wayland: Log out and log back in"
echo ""
echo "To verify it's working:"
echo "  gdbus call --session --dest org.gnome.Shell --object-path /org/hdepanel/WindowList --method org.hdepanel.WindowList.GetWindows"
echo ""

