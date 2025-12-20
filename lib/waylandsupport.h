#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QString>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QSocketNotifier>

#include <wayland-client.h>
#include "windowmanagers/windowmanager.h"
#include "waylandwindow.h"

#ifdef HDE_HAVE_WAYLAND
struct zwlr_foreign_toplevel_manager_v1;
struct zwlr_foreign_toplevel_handle_v1;
#endif

class WaylandSupport : public QObject
{
    Q_OBJECT

public:
    explicit WaylandSupport(QObject* parent = nullptr);
    ~WaylandSupport();

    bool initialize();
    bool isAvailable();
    QList<WaylandWindow> getAllWindows();
    WaylandWindow getWindowInfo(void* surface);
    bool activateWindow(const QString& appId);
    bool closeWindow(const QString& appId);
    bool minimizeWindow(const QString& appId);
    bool maximizeWindow(const QString& appId);
    bool unmaximizeWindow(const QString& appId);
    bool moveWindowToWorkspace(const QString& appId, int workspace);
    bool moveWindowToMonitor(const QString& appId, int monitor);
    bool activateWindow(void* surface);
    bool closeWindow(void* surface);

    // Window manager information
    QString getWindowManagerName() const;
    WindowManager::Type getWindowManagerType() const;
    QString getWindowManagerVersion() const;
    bool supportsWorkspaces() const;
    bool supportsMultipleMonitors() const;
    int getCurrentWorkspace() const;
    int getWorkspaceCount() const;
    QStringList getWorkspaceNames() const;
    int getMonitorCount() const;
    QStringList getMonitorNames() const;

#ifdef HDE_HAVE_WAYLAND
    // wlroots foreign-toplevel protocol callbacks (used in C listeners).
    // These must be public so the C listener table can reference them.
    static void handleToplevelManagerToplevel(void* data, zwlr_foreign_toplevel_manager_v1* manager, zwlr_foreign_toplevel_handle_v1* toplevel);
    static void handleToplevelManagerFinished(void* data, zwlr_foreign_toplevel_manager_v1* manager);
    static void handleToplevelTitle(void* data, zwlr_foreign_toplevel_handle_v1* toplevel, const char* title);
    static void handleToplevelAppId(void* data, zwlr_foreign_toplevel_handle_v1* toplevel, const char* app_id);
    static void handleToplevelOutputEnter(void* data, zwlr_foreign_toplevel_handle_v1* toplevel, wl_output* output);
    static void handleToplevelOutputLeave(void* data, zwlr_foreign_toplevel_handle_v1* toplevel, wl_output* output);
    static void handleToplevelState(void* data, zwlr_foreign_toplevel_handle_v1* toplevel, wl_array* state);
    static void handleToplevelDone(void* data, zwlr_foreign_toplevel_handle_v1* toplevel);
    static void handleToplevelClosed(void* data, zwlr_foreign_toplevel_handle_v1* toplevel);
    static void handleToplevelParent(void* data, zwlr_foreign_toplevel_handle_v1* toplevel, zwlr_foreign_toplevel_handle_v1* parent);
#endif

signals:
    void windowsUpdated(const QList<WaylandWindow>& windows);
    void windowCreated(const WaylandWindow& window);
    void windowClosed(const WaylandWindow& window);
    void windowActivated(const WaylandWindow& window);
    void workspaceChanged(int workspace);
    void monitorChanged(int monitor);

private slots:
    void updateWindows();
    void onWindowManagerWindowsUpdated(const QList<WaylandWindow>& windows);
    void onWindowManagerWindowCreated(const WaylandWindow& window);
    void onWindowManagerWindowClosed(const WaylandWindow& window);
    void onWindowManagerWindowActivated(const WaylandWindow& window);
    void onWindowManagerWorkspaceChanged(int workspace);
    void onWindowManagerMonitorChanged(int monitor);
#ifdef HDE_HAVE_WAYLAND
    void onWaylandReadable();
#endif

private:
    void handleRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
    void scheduleEmitWindowsUpdated();
    QList<WaylandWindow> getWindowsFromForeignToplevel() const;

    // Helper functions
    bool isGuiApplication(const QString& comm, const QString& cmd);
    QString getApplicationTitle(const QString& comm, const QString& cmd);
    QString getApplicationIcon(const QString& appId, const QString& wmClass = QString()) const;
    
    // Legacy GNOME Shell D-Bus integration (for backward compatibility)
    QList<WaylandWindow> getWindowsFromExtension();     // Safe method using our extension
    QList<WaylandWindow> getWindowsFromGnomeShell();    // Fallback using unsafe eval

    static const wl_registry_listener registryListener;

    // Window manager system
    WindowManager* m_windowManager;
    bool m_useWindowManager;

    // Legacy Wayland support (for backward compatibility)
    bool m_initialized;
    wl_display* m_display;
    bool m_ownsDisplay;
    wl_registry* m_registry;
    wl_compositor* m_compositor;
    wl_interface* m_xdg_wm_base;
    QTimer* m_updateTimer;
#ifdef HDE_HAVE_WAYLAND
    wl_seat* m_seat = nullptr;
    QSocketNotifier* m_displayNotifier = nullptr;
#endif

#ifdef HDE_HAVE_WAYLAND
    // wlroots: wlr-foreign-toplevel-management (used by Wayfire and other wlroots compositors)
    struct ForeignToplevelData {
        zwlr_foreign_toplevel_handle_v1* handle = nullptr;
        WaylandWindow window;
        bool done = false;
    };

    zwlr_foreign_toplevel_manager_v1* m_foreignToplevelManager = nullptr;
    QMap<zwlr_foreign_toplevel_handle_v1*, ForeignToplevelData*> m_foreignToplevels;
    bool m_emitScheduled = false;
    mutable QMutex m_foreignMutex;
#endif
};
