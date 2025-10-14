#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QString>
#include <QMap>
#include <QSet>

#include <wayland-client.h>

// Comprehensive representation of a "window" for panels/taskbars
struct WaylandWindow {
    // Basic identification
    QString title;
    QString appId;
    QString iconName;
    QString wmClass;
    QString wmInstanceClass;
    QString role;
    
    // Workspace/Desktop information
    int workspaceIndex;
    QString workspaceName;
    bool onAllWorkspaces;
    bool isOnCurrentWorkspace;
    
    // Screen/Monitor information
    int monitorIndex;
    
    // Geometry
    int x;
    int y;
    int width;
    int height;
    
    // Window state
    bool visible;
    bool focused;
    bool minimized;
    bool maximized;
    bool maximizedHorizontally;
    bool maximizedVertically;
    
    // Window flags
    bool demandsAttention;
    bool urgent;
    bool skipTaskbar;
    bool skipPager;
    bool decorated;
    bool resizable;
    bool moveable;
    
    // Client type
    bool isWayland;
    bool isX11;
    QString clientType;
    
    // Process information
    int pid;
    QString sandboxedAppId;
    
    // Visual properties
    double opacity;
    
    // Group information
    bool hasGroup;
    unsigned long groupLeaderId;
    
    // Timestamps
    unsigned long createdTime;
    unsigned long focusTime;
    
    // Legacy fields for compatibility
    void* surface;
    void* toplevel;
};

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

signals:
    void windowsUpdated(const QList<WaylandWindow>& windows);

private slots:
    void updateWindows();

private:
    void handleRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);

    // Helper functions
    bool isGuiApplication(const QString& comm, const QString& cmd);
    QString getApplicationTitle(const QString& comm, const QString& cmd);
    QString getApplicationIcon(const QString& appId, const QString& wmClass = QString());
    
    // GNOME Shell D-Bus integration
    QList<WaylandWindow> getWindowsFromExtension();     // Safe method using our extension
    QList<WaylandWindow> getWindowsFromGnomeShell();    // Fallback using unsafe eval

    static const wl_registry_listener registryListener;

    bool m_initialized;
    wl_display* m_display;
    wl_registry* m_registry;
    wl_compositor* m_compositor;
    wl_interface* m_xdg_wm_base;
    QTimer* m_updateTimer;
};
