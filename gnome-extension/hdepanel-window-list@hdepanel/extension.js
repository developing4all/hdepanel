/* HDEPanel Window List Extension
 * Provides window list information to HDEPanel via D-Bus
 * Compatible with GNOME Shell 45–48 (new Extension API)
 */

import Gio from 'gi://Gio';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import { Extension } from 'resource:///org/gnome/shell/extensions/extension.js';

const DBUS_PATH = '/org/hdepanel/WindowList';
const DBUS_NAME = 'org.hdepanel.WindowList';
const DBUS_XML = `
<node>
  <interface name="org.hdepanel.WindowList">
    <method name="GetWindows">
      <arg type="s" direction="out" name="windows"/>
    </method>
    <method name="ActivateWindow">
      <arg type="s" direction="in" name="appId"/>
      <arg type="b" direction="out" name="success"/>
    </method>
    <method name="CloseWindow">
      <arg type="s" direction="in" name="appId"/>
      <arg type="b" direction="out" name="success"/>
    </method>
    <signal name="WindowsChanged"/>
  </interface>
</node>`;

// ────────────────────────────────────────────────

export default class HDEPanelWindowListExtension extends Extension {
    enable() {
        this._dbusImpl = null;
        this._nameOwnerId = 0;
        this._windowTracker = Shell.WindowTracker.get_default();
        this._signalIds = [];

        try {
            // 1️⃣ Export D-Bus interface
            this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(DBUS_XML, this);
            this._dbusImpl.export(Gio.DBus.session, DBUS_PATH);

            // 2️⃣ Own the session bus name
            this._nameOwnerId = Gio.DBus.session.own_name(
                DBUS_NAME,
                Gio.BusNameOwnerFlags.NONE,
                null,
                null
            );

            // 3️⃣ Connect GNOME Shell signals
            const display = global.display;
            this._signalIds.push(display.connect('window-created', this._onWindowsChanged.bind(this)));
            this._signalIds.push(display.connect('restacked', this._onWindowsChanged.bind(this)));
            this._signalIds.push(display.connect('window-demands-attention', this._onWindowsChanged.bind(this)));

            const wm = display.get_workspace_manager();
            this._signalIds.push(wm.connect('active-workspace-changed', this._onWindowsChanged.bind(this)));

            log('[HDEPanel] Window List extension enabled');
        } catch (e) {
            logError(e, '[HDEPanel] Failed to enable extension');
        }
    }

    disable() {
        // Disconnect signals
        for (const id of this._signalIds) {
            try { global.display.disconnect(id); } catch {}
        }
        this._signalIds = [];

        // Unown the name
        if (this._nameOwnerId) {
            Gio.DBus.session.unown_name(this._nameOwnerId);
            this._nameOwnerId = 0;
        }

        // Unexport interface
        if (this._dbusImpl) {
            try { this._dbusImpl.unexport(); } catch {}
            this._dbusImpl = null;
        }

        log('[HDEPanel] Window List extension disabled');
    }

    // ─────── D-Bus exposed methods ───────

    _onWindowsChanged() {
        if (!this._dbusImpl)
            return;

        try {
            this._dbusImpl.emit_signal('WindowsChanged', null);
        } catch (e) {
            logError(e, '[HDEPanel] Failed to emit WindowsChanged');
        }
    }

    GetWindows() {
        const tracker = this._windowTracker;
        const workspace = global.workspace_manager.get_active_workspace();
        const windows = [];

        for (const actor of global.get_window_actors()) {
            const w = actor.get_meta_window();
            if (!w)
                continue;

            const type = w.get_window_type();
            if (type !== Meta.WindowType.NORMAL)
                continue;

            const app = tracker.get_window_app(w);
            const appId = app ? app.get_id() : '';
            const wmClass = w.get_wm_class() || '';

            if (this._isSystemWindow(w, appId))
                continue;

            // Get workspace information
            const windowWorkspace = w.get_workspace();
            const workspaceIndex = windowWorkspace ? windowWorkspace.index() : -1;
            const workspaceName = windowWorkspace ? `Workspace ${workspaceIndex + 1}` : '';
            
            // Get monitor information
            const monitor = w.get_monitor();
            const monitorIndex = monitor !== -1 ? monitor : -1;
            
            // Get window geometry
            const rect = w.get_frame_rect();
            
            // Get window state
            const maximized = w.get_maximized();
            const isMaximizedHorizontally = (maximized & Meta.MaximizeFlags.HORIZONTAL) !== 0;
            const isMaximizedVertically = (maximized & Meta.MaximizeFlags.VERTICAL) !== 0;
            
            // Get window class and role
            const wmInstanceClass = w.get_wm_class_instance() || '';
            const role = w.get_role() || '';
            
            // Get window state flags (with fallbacks for different GNOME versions)
            const demandsAttention = (typeof w.demands_attention === 'function') ? w.demands_attention() : false;
            const skipTaskbar = (typeof w.is_skip_taskbar === 'function') ? w.is_skip_taskbar() : false;
            const skipPager = (typeof w.is_skip_pager === 'function') ? w.is_skip_pager() : false;
            const onAllWorkspaces = (typeof w.is_on_all_workspaces === 'function') ? w.is_on_all_workspaces() : false;
            const urgent = (typeof w.urgent === 'boolean') ? w.urgent : false;
            
            // Get client type (Wayland vs X11)
            const clientType = w.get_client_type();
            const isWayland = clientType === Meta.WindowClientType.WAYLAND;
            const isX11 = clientType === Meta.WindowClientType.X11;
            
            // Get PID and process info
            const pid = w.get_pid();
            const sandboxedAppId = w.get_sandboxed_app_id() || '';
            
            // Get window decorations (with fallbacks for different GNOME versions)
            const decorated = (typeof w.get_decorated === 'function') ? w.get_decorated() : true;
            const resizable = (typeof w.get_resizable === 'function') ? w.get_resizable() : true;
            const moveable = (typeof w.get_moveable === 'function') ? w.get_moveable() : true;
            
            // Get window opacity (with fallbacks for different GNOME versions)
            const opacity = (typeof w.get_opacity === 'function') ? w.get_opacity() : 1.0;
            
            // Get window group (with fallbacks for different GNOME versions)
            const windowGroup = (typeof w.get_group === 'function') ? w.get_group() : null;
            const groupLeader = windowGroup && (typeof windowGroup.get_group_leader === 'function') ? windowGroup.get_group_leader() : null;
            
            // Get application icon
            let iconName = '';
            if (app) {
                iconName = app.get_icon() ? app.get_icon().to_string() : '';
            }
            if (!iconName && wmClass) {
                // Fallback to wm_class for icon detection
                iconName = wmClass.toLowerCase();
            }
            if (!iconName) {
                iconName = 'application-x-executable';
            }
            
            windows.push({
                // Basic identification
                id: w.get_id(),
                title: w.get_title() || '',
                app_id: appId.replace('.desktop', ''),
                wm_class: wmClass,
                wm_instance_class: wmInstanceClass,
                role: role,
                icon_name: iconName,
                
                // Workspace/Desktop information
                workspace_index: workspaceIndex,
                workspace_name: workspaceName,
                on_all_workspaces: onAllWorkspaces,
                
                // Screen/Monitor information
                monitor_index: monitorIndex,
                is_on_current_workspace: windowWorkspace === workspace,
                
                // Geometry
                x: rect.x,
                y: rect.y,
                width: rect.width,
                height: rect.height,
                
                // Window state
                visible: !w.minimized && w.showing_on_its_workspace(),
                focused: w.has_focus(),
                minimized: w.minimized,
                maximized: maximized !== 0,
                maximized_horizontally: isMaximizedHorizontally,
                maximized_vertically: isMaximizedVertically,
                
                // Window flags
                demands_attention: demandsAttention,
                urgent: urgent,
                skip_taskbar: skipTaskbar,
                skip_pager: skipPager,
                decorated: decorated,
                resizable: resizable,
                moveable: moveable,
                
                // Client type
                is_wayland: isWayland,
                is_x11: isX11,
                client_type: isWayland ? 'wayland' : (isX11 ? 'x11' : 'unknown'),
                
                // Process information
                pid: pid,
                sandboxed_app_id: sandboxedAppId,
                
                // Visual properties
                opacity: opacity,
                
                // Group information
                has_group: windowGroup !== null,
                group_leader_id: groupLeader ? groupLeader.get_id() : null,
                
                // Timestamps (with fallbacks for different GNOME versions)
                created_time: (typeof w.get_user_time === 'function') ? w.get_user_time() : 0,
                focus_time: (typeof w.get_focus_time === 'function') ? w.get_focus_time() : 0,
            });
        }

        return JSON.stringify(windows);
    }

    ActivateWindow(appId) {
        const workspace = global.workspace_manager.get_active_workspace();
        
        for (const actor of global.get_window_actors()) {
            const w = actor.get_meta_window();
            if (!w)
                continue;

            const type = w.get_window_type();
            if (type !== Meta.WindowType.NORMAL)
                continue;

            const app = this._windowTracker.get_window_app(w);
            const windowAppId = app ? app.get_id().replace('.desktop', '') : '';
            const wmClass = w.get_wm_class() || '';

            if (this._isSystemWindow(w, windowAppId))
                continue;

            // Match by appId or wmClass
            if (windowAppId === appId || wmClass === appId) {
                // Activate the window
                w.activate(global.get_current_time());
                log(`[HDEPanel] Activated window: ${w.get_title()} (${appId})`);
                return true;
            }
        }

        log(`[HDEPanel] No window found for appId: ${appId}`);
        return false;
    }

    CloseWindow(appId) {
        for (const actor of global.get_window_actors()) {
            const w = actor.get_meta_window();
            if (!w)
                continue;

            const type = w.get_window_type();
            if (type !== Meta.WindowType.NORMAL)
                continue;

            const app = this._windowTracker.get_window_app(w);
            const windowAppId = app ? app.get_id().replace('.desktop', '') : '';
            const wmClass = w.get_wm_class() || '';

            if (this._isSystemWindow(w, windowAppId))
                continue;

            // Match by appId or wmClass
            if (windowAppId === appId || wmClass === appId) {
                // Close the window
                w.delete(global.get_current_time());
                log(`[HDEPanel] Closed window: ${w.get_title()} (${appId})`);
                return true;
            }
        }

        log(`[HDEPanel] No window found to close for appId: ${appId}`);
        return false;
    }

    _isSystemWindow(w, appId) {
        if (w.is_skip_taskbar())
            return true;

        const t = w.get_window_type();
        if (t === Meta.WindowType.DOCK || t === Meta.WindowType.DESKTOP)
            return true;

        if (appId && appId.includes('hdepanel'))
            return true;

        const wmClass = (w.get_wm_class() || '').toLowerCase();
        return wmClass.includes('hdepanel');
    }
}
