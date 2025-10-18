/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Authors:
 *   Haydar Alkaduhimi <haydar@developing4all.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef WAYLANDLAYERSHELL_H
#define WAYLANDLAYERSHELL_H

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QWindow>
#include <cstdint>

#include <wayland-client.h>

#ifdef __cplusplus
extern "C" {
#define namespace namespace_
#endif
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#ifdef __cplusplus
#undef namespace
}
#endif

/**
 * @brief Wayland layer-shell helper to pin the panel window
 */
class WaylandLayerShell : public QObject
{
    Q_OBJECT

public:
    enum class Layer : uint32_t {
        Background = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
        Bottom = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
        Top = ZWLR_LAYER_SHELL_V1_LAYER_TOP,
        Overlay = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY
    };

    enum class Anchor : uint32_t {
        Top = 1u << 0,
        Bottom = 1u << 1,
        Left = 1u << 2,
        Right = 1u << 3
    };

    enum class KeyboardFocus : uint32_t {
        None = 0,
        Exclusive = 1,
        OnDemand = 2
    };

    explicit WaylandLayerShell(QObject* parent = nullptr);
    ~WaylandLayerShell() override;

    bool initialize();
    bool isAvailable() const;

    void setWindow(QWindow* window);
    void setLayer(Layer layer);
    void setAnchor(Anchor anchor);
    void setAnchorMask(uint32_t anchorMask);
    void setExclusiveZone(int zone);
    void setMargin(int top, int right, int bottom, int left);
    void setKeyboardInteractivity(bool interactive);
    void setKeyboardFocus(KeyboardFocus focus);
    void setSize(const QSize& size);
    void setPosition(const QPoint& position);
    void setNamespace(const QString& namespace_);

    void commit();

signals:
    void configureRequested(uint32_t serial, uint32_t width, uint32_t height);
    void closed();

private:
    static void configureCallback(void* data, zwlr_layer_surface_v1* surface,
                                  uint32_t serial, uint32_t width, uint32_t height);
    static void closedCallback(void* data, zwlr_layer_surface_v1* surface);

    void handleConfigure(uint32_t serial, uint32_t width, uint32_t height);
    void handleClosed();
    void handleRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version);

    uint32_t anchorMask() const { return m_anchorMask; }

    bool m_initialized = false;

    wl_display* m_display = nullptr;      // borrowed from Qt
    wl_registry* m_registry = nullptr;
    wl_compositor* m_compositor = nullptr; // borrowed from Qt
    zwlr_layer_shell_v1* m_layerShell = nullptr;
    zwlr_layer_surface_v1* m_layerSurface = nullptr;
    wl_surface* m_surface = nullptr;      // borrowed from Qt
    wl_output* m_output = nullptr;        // borrowed from Qt

    QWindow* m_window = nullptr;

    Layer m_layer = Layer::Top;
    uint32_t m_anchorMask = static_cast<uint32_t>(Anchor::Top);
    int m_exclusiveZone = 0;
    int m_marginTop = 0;
    int m_marginRight = 0;
    int m_marginBottom = 0;
    int m_marginLeft = 0;
    bool m_keyboardInteractive = false;
    KeyboardFocus m_keyboardFocus = KeyboardFocus::None;
    QString m_namespace = "hdepanel";
    QSize m_size = QSize(1920, 48);
    QPoint m_position = QPoint(0, 0);

    bool m_configured = false;
    uint32_t m_pendingSerial = 0;
    QSize m_pendingSize = QSize(1920, 48);

    static const zwlr_layer_surface_v1_listener s_layerSurfaceListener;
};

#endif // WAYLANDLAYERSHELL_H
