#pragma once

#include <QObject>
#include <QList>
#include <QTimer>
#include <QString>
#include <QMap>
#include <QSet>

#include <wayland-client.h>

// Simple representation of a "window" for panels/taskbars
struct WaylandWindow {
    QString title;
    QString appId;
    QString iconName;
    bool visible;
    bool focused;
    int x;
    int y;
    int width;
    int height;
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
    QList<WaylandWindow> getWindowsFromGnomeShell();

    static const wl_registry_listener registryListener;

    bool m_initialized;
    wl_display* m_display;
    wl_registry* m_registry;
    wl_compositor* m_compositor;
    wl_interface* m_xdg_wm_base;
    QTimer* m_updateTimer;
};
