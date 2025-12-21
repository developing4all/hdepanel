# HDEPanel D-Bus Keybinding Interface

HDEPanel exposes a D-Bus interface that allows external applications (like Openbox, Wayfire, or other window managers) to trigger panel actions via keybindings.

## D-Bus Service Details

- **Service Name**: `com.developing4all.hdepanel`
- **Object Path**: `/com/developing4all/hdepanel`
- **Interface**: `com.developing4all.hdepanel`

## Available Methods

### ShowStartMenu()

Shows the start menu if a StartApplet is present on any panel.

**Signature**: `() -> b`

**Returns**: `true` if a start menu was found and shown, `false` otherwise

**Example**:
```bash
gdbus call --session \
  --dest com.developing4all.hdepanel \
  --object-path /com/developing4all/hdepanel \
  --method com.developing4all.hdepanel.ShowStartMenu
```

### ToggleStartMenu()

Toggles the start menu (shows if hidden, hides if shown).

**Signature**: `() -> b`

**Returns**: `true` if a start menu was found and toggled, `false` otherwise

**Example**:
```bash
gdbus call --session \
  --dest com.developing4all.hdepanel \
  --object-path /com/developing4all/hdepanel \
  --method com.developing4all.hdepanel.ToggleStartMenu
```

### HideStartMenu()

Hides the start menu if it's currently shown.

**Signature**: `() -> b`

**Returns**: `true` if a start menu was found and hidden, `false` otherwise

**Example**:
```bash
gdbus call --session \
  --dest com.developing4all.hdepanel \
  --object-path /com/developing4all/hdepanel \
  --method com.developing4all.hdepanel.HideStartMenu
```

## Integration Examples

### Openbox (rc.xml)

Add a keybinding to your `~/.config/openbox/rc.xml`:

**Finding the correct Super key name:**

If `W` doesn't work for the Super key in your Openbox setup, you can find the correct key name using one of these methods:

1. **Using `xev`** (X11 only):
   ```bash
   xev | grep -A2 --line-buffered '^KeyRelease' | sed -n '/keycode /p'
   ```
   Press the Super key and note the keycode. Then use it in Openbox with the `keycode` attribute (not `key`):
   ```xml
   <keybind keycode="133">  <!-- Replace 133 with your keycode -->
   ```

2. **Using `obkey`** (Openbox keybinding tool):
   ```bash
   obkey ~/.config/openbox/rc.xml
   ```
   This GUI tool will help you find and set the correct keybinding.

3. **Common alternatives:**
   - Try `Super_L` or `Super_R` for left/right Super keys
   - Try `Meta_L` or `Meta_R` if Super is mapped to Meta
   - Use the keycode directly (e.g., `133` for left Super, `134` for right Super)

**Example keybindings:**

Super key alone (using keycode - **use `keycode` attribute, not `key`**):
```xml
<keybind keycode="133">
  <action name="Execute">
    <command>gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu</command>
  </action>
</keybind>
```

Super key alone (using keysym):
```xml
<keybind key="Super_L">
  <action name="Execute">
    <command>gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu</command>
  </action>
</keybind>
```

Super + S (if `W` works for Super):
```xml
<keybind key="W-s">
  <action name="Execute">
    <command>gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu</command>
  </action>
</keybind>
```

**Making Super key alone work:**

Openbox has difficulty binding modifier keys (like Super) alone. If you want Super key alone to work, use `xcape` or `ksuperkey` to make Super key alone trigger another key, then bind that key in Openbox.

**Option A: Using `xcape` (Recommended)**

1. **Install xcape:**
   ```bash
   sudo apt install xcape  # Debian/Ubuntu
   # or
   sudo pacman -S xcape    # Arch
   ```

2. **Add to Openbox autostart** (`~/.config/openbox/autostart`):
   ```bash
   xcape -e 'Super_L=Menu' &
   ```
   This makes Super key alone trigger Menu (an unused function key).

3. **Bind Menu in rc.xml:**
   ```xml
   <keybind key="Menu">
     <action name="Execute">
       <command>gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu</command>
     </action>
   </keybind>
   ```

4. **Restart Openbox:**
   ```bash
   openbox --reconfigure
   # Or restart your session
   ```

**Option B: Using `ksuperkey`**

1. **Install ksuperkey:**
   ```bash
   sudo apt install ksuperkey  # Debian/Ubuntu
   # or
   sudo pacman -S ksuperkey    # Arch
   ```

2. **Add to Openbox autostart** (`~/.config/openbox/autostart`):
   ```bash
   ksuperkey -e 'Super_L=F13' &
   ```

3. **Bind F13 in rc.xml** (same as Option A, step 3)

**Note:** Place the keybind inside the `<keyboard>` section of your `rc.xml` file. After editing, reload Openbox with:
```bash
openbox --reconfigure
```

**Troubleshooting:**

If the keybinding still doesn't work, try these steps:

1. **Verify the D-Bus service is running:**
   ```bash
   gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu
   ```
   If this works manually, the service is fine and the issue is with the keybinding.

2. **Check for XML syntax errors:**
   ```bash
   openbox --reconfigure 2>&1 | grep -i error
   ```
   Fix any XML errors before proceeding.

3. **Try wrapping the command in a shell:**
   ```xml
   <keybind keycode="133">
     <action name="Execute">
       <command>sh -c "gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu"</command>
     </action>
   </keybind>
   ```

4. **Check if Super key is grabbed by another application:**
   ```bash
   xev | grep -A2 --line-buffered '^KeyRelease' | sed -n '/keycode /p'
   ```
   Press Super and see if it's being captured. If nothing appears, another app might be grabbing it.

5. **Try using a different key first to test:**
   ```xml
   <keybind key="F12">
     <action name="Execute">
       <command>gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu</command>
     </action>
   </keybind>
   ```
   If F12 works, the issue is specific to the Super key binding.

6. **Check for conflicting keybinds:**
   Search your `rc.xml` for other uses of keycode 133 or Super_L to ensure there's no conflict.

7. **Try using `obkey` GUI tool:**
   ```bash
   obkey ~/.config/openbox/rc.xml
   ```
   This tool can help set up the keybinding correctly and verify the syntax.

8. **Use a key combination instead of Super alone:**
   Some Openbox versions have issues binding modifier keys alone. Try Super + Space:
   ```xml
   <keybind key="W-space">
     <action name="Execute">
       <command>sh -c "gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu"</command>
     </action>
   </keybind>
   ```
   Or use Super + S:
   ```xml
   <keybind key="W-s">
     <action name="Execute">
       <command>sh -c "gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu"</command>
     </action>
   </keybind>
   ```

9. **Use a wrapper script:**
   Create `/tmp/hdepanel_show_startmenu.sh`:
   ```bash
   #!/bin/bash
   gdbus call --session \
     --dest com.developing4all.hdepanel \
     --object-path /com/developing4all/hdepanel \
     --method com.developing4all.hdepanel.ShowStartMenu > /dev/null 2>&1
   ```
   Make it executable: `chmod +x /tmp/hdepanel_show_startmenu.sh`
   
   Then in rc.xml:
   ```xml
   <keybind keycode="133">
     <action name="Execute">
       <command>/tmp/hdepanel_show_startmenu.sh</command>
     </action>
   </keybind>
   ```

10. **Try restarting Openbox instead of just reconfiguring:**
    Sometimes a full restart is needed:
    ```bash
    # Log out and log back in, or:
    killall openbox && openbox &
    ```

11. **Try using `start` action instead of `Execute`:**
    ```xml
    <keybind keycode="133">
      <action name="Start">
        <command>sh -c "gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu"</command>
      </action>
    </keybind>
    ```

### Wayfire (wayfire.ini)

Add a keybinding to your `~/.config/wayfire.ini`:

```ini
[command]
binding_super_s = gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu
```

### Sway/i3 (config)

Add a keybinding to your Sway or i3 config:

```
bindsym $mod+s exec gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method com.developing4all.hdepanel.ShowStartMenu
```

### Using Python

```python
import dbus

bus = dbus.SessionBus()
obj = bus.get_object('com.developing4all.hdepanel', '/com/developing4all/hdepanel')
iface = dbus.Interface(obj, 'com.developing4all.hdepanel')
result = iface.ShowStartMenu()
print(f"Start menu shown: {result}")
```

### Using qdbus (Qt tool)

```bash
qdbus com.developing4all.hdepanel /com/developing4all/hdepanel com.developing4all.hdepanel.ShowStartMenu
```

## Testing

To test if the service is available:

```bash
# Check if service is registered
gdbus introspect --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel

# List available methods
gdbus call --session --dest com.developing4all.hdepanel --object-path /com/developing4all/hdepanel --method org.freedesktop.DBus.Introspectable.Introspect
```

## Notes

- The D-Bus service is automatically registered when HDEPanel starts
- The service will only work if HDEPanel is running
- If multiple panels have StartApplets, the first one found will be triggered
- The start menu will appear on the screen where the panel with the StartApplet is located

