#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QString>
#include <QMap>
#include <QSet>

#include <wayland-client.h>
#include "windowmanagers/windowmanager.h"
#include "waylandwindow.h"

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

private:
    void handleRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);

    // Helper functions
    bool isGuiApplication(const QString& comm, const QString& cmd);
    QString getApplicationTitle(const QString& comm, const QString& cmd);
    QString getApplicationIcon(const QString& appId, const QString& wmClass = QString());
    
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
    wl_registry* m_registry;
    wl_compositor* m_compositor;
    wl_interface* m_xdg_wm_base;
    QTimer* m_updateTimer;
};
