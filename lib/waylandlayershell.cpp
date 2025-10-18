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

#include "waylandlayershell.h"
#include <QDebug>
#include <QGuiApplication>
#include <QWindow>
#include <QScreen>
#include <QWidget>
// Use Qt6 native interface approach

WaylandLayerShell::WaylandLayerShell(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_display(nullptr)
    , m_registry(nullptr)
    , m_compositor(nullptr)
    , m_layerShell(nullptr)
    , m_layerSurface(nullptr)
    , m_surface(nullptr)
    , m_output(nullptr)
    , m_window(nullptr)
    , m_layer(Layer::Top)
    , m_anchorMask(static_cast<uint32_t>(Anchor::Top))
    , m_exclusiveZone(0)
    , m_marginTop(0)
    , m_marginRight(0)
    , m_marginBottom(0)
    , m_marginLeft(0)
    , m_keyboardInteractive(false)
    , m_keyboardFocus(KeyboardFocus::None)
    , m_namespace("hdepanel")
    , m_size(1920, 48)
    , m_position(0, 0)
    , m_configured(false)
    , m_pendingSerial(0)
    , m_pendingSize(1920, 48)
{
}

WaylandLayerShell::~WaylandLayerShell()
{
    // Clean up in reverse order of creation
    if (m_layerSurface) {
        zwlr_layer_surface_v1_destroy(m_layerSurface);
        m_layerSurface = nullptr;
    }
    if (m_surface) {
        wl_surface_destroy(m_surface);
        m_surface = nullptr;
    }
    if (m_layerShell) {
        zwlr_layer_shell_v1_destroy(m_layerShell);
        m_layerShell = nullptr;
    }
    if (m_compositor) {
        wl_compositor_destroy(m_compositor);
        m_compositor = nullptr;
    }
    if (m_output) {
        wl_output_destroy(m_output);
        m_output = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }
}

bool WaylandLayerShell::initialize()
{
    if (m_initialized) {
        return true;
    }

    // Connect to Wayland display
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        qDebug() << "WaylandLayerShell: Failed to connect to Wayland display";
        return false;
    }

    // Get registry
    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        qDebug() << "WaylandLayerShell: Failed to get registry";
        return false;
    }

    // Set up registry listener
    static const wl_registry_listener registryListener = {
        [](void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
            static_cast<WaylandLayerShell*>(data)->handleRegistryGlobal(registry, name, interface, version);
        },
        [](void*, wl_registry*, uint32_t) {}
    };

    wl_registry_add_listener(m_registry, &registryListener, this);
    wl_display_roundtrip(m_display);

    if (!m_compositor || !m_layerShell) {
        qDebug() << "WaylandLayerShell: Required interfaces not available";
        return false;
    }

    m_initialized = true;
    qDebug() << "WaylandLayerShell: Initialized successfully";
    return true;
}

bool WaylandLayerShell::isAvailable() const
{
    return m_initialized && m_compositor && m_layerShell;
}

void WaylandLayerShell::setWindow(QWindow* window)
{
    m_window = window;
    if (window && m_layerShell) {
        qDebug() << "WaylandLayerShell: Setting up layer-shell for Qt window";
        
        // KNOWN LIMITATION: We're creating a separate surface for layer-shell
        // because accessing Qt's native Wayland surface requires private headers
        // that are not available in standard Qt installations.
        // 
        // This means the compositor will position the layer shell surface correctly,
        // but the Qt window will render to a different surface and won't follow
        // the layer shell positioning.
        // 
        // To fix this properly, you would need to either:
        // 1. Install Qt development packages with private headers
        // 2. Use a custom Qt Wayland compositor plugin
        // 3. Rewrite the panel to not use QWidget/QWindow
        
        qDebug() << "WaylandLayerShell: Creating separate layer-shell surface (Qt window positioning won't work)";
        m_surface = wl_compositor_create_surface(m_compositor);
        
        if (m_surface) {
            qDebug() << "WaylandLayerShell: Using surface for layer-shell";
            
            // Create layer surface using the surface
            m_layerSurface = zwlr_layer_shell_v1_get_layer_surface(
                m_layerShell, m_surface, m_output, 
                static_cast<uint32_t>(m_layer), m_namespace.toUtf8().constData());
            
            if (m_layerSurface) {
                qDebug() << "WaylandLayerShell: Created layer surface with namespace:" << m_namespace;
                zwlr_layer_surface_v1_add_listener(m_layerSurface, &s_layerSurfaceListener, this);
                
                // Set initial properties
                zwlr_layer_surface_v1_set_anchor(m_layerSurface, m_anchorMask);
                zwlr_layer_surface_v1_set_exclusive_zone(m_layerSurface, m_exclusiveZone);
                zwlr_layer_surface_v1_set_margin(m_layerSurface, m_marginTop, m_marginRight, m_marginBottom, m_marginLeft);
                zwlr_layer_surface_v1_set_keyboard_interactivity(m_layerSurface, m_keyboardInteractive ? 1 : 0);
                zwlr_layer_surface_v1_set_size(m_layerSurface, m_size.width(), m_size.height());
                
                qDebug() << "WaylandLayerShell: Layer surface configured for positioning";
                
                // Commit to trigger configure event
                commit();
            } else {
                qDebug() << "WaylandLayerShell: Failed to create layer surface";
            }
        } else {
            qDebug() << "WaylandLayerShell: Failed to create surface";
        }
    }
}

void WaylandLayerShell::setLayer(Layer layer)
{
    m_layer = layer;
    // Layer can only be set when creating the surface
}

void WaylandLayerShell::setAnchor(Anchor anchor)
{
    m_anchorMask = static_cast<uint32_t>(anchor);
    if (m_layerSurface) {
        zwlr_layer_surface_v1_set_anchor(m_layerSurface, m_anchorMask);
    }
}

void WaylandLayerShell::setAnchorMask(uint32_t anchorMask)
{
    m_anchorMask = anchorMask;
    if (m_layerSurface) {
        zwlr_layer_surface_v1_set_anchor(m_layerSurface, m_anchorMask);
    }
}

void WaylandLayerShell::setExclusiveZone(int zone)
{
    m_exclusiveZone = zone;
    if (m_layerSurface) {
        zwlr_layer_surface_v1_set_exclusive_zone(m_layerSurface, zone);
    }
}

void WaylandLayerShell::setMargin(int top, int right, int bottom, int left)
{
    m_marginTop = top;
    m_marginRight = right;
    m_marginBottom = bottom;
    m_marginLeft = left;
    
    if (m_layerSurface) {
        zwlr_layer_surface_v1_set_margin(m_layerSurface, top, right, bottom, left);
    }
}

void WaylandLayerShell::setKeyboardInteractivity(bool interactive)
{
    m_keyboardInteractive = interactive;
    if (m_layerSurface) {
        zwlr_layer_surface_v1_set_keyboard_interactivity(m_layerSurface, interactive ? 1 : 0);
    }
}

void WaylandLayerShell::setKeyboardFocus(KeyboardFocus focus)
{
    m_keyboardFocus = focus;
    // Keyboard focus is handled through keyboard interactivity
    // This is a higher-level abstraction for easier management
    switch (focus) {
        case KeyboardFocus::None:
            setKeyboardInteractivity(false);
            break;
        case KeyboardFocus::Exclusive:
        case KeyboardFocus::OnDemand:
            setKeyboardInteractivity(true);
            break;
    }
}

void WaylandLayerShell::setNamespace(const QString& namespace_)
{
    m_namespace = namespace_;
    // Namespace can only be set when creating the surface
    // This is used for window identification by external tools
}

void WaylandLayerShell::setSize(const QSize& size)
{
    m_size = size;
    if (m_layerSurface) {
        zwlr_layer_surface_v1_set_size(m_layerSurface, size.width(), size.height());
    }
}

void WaylandLayerShell::setPosition(const QPoint& position)
{
    m_position = position;
    // Position is handled by the compositor based on anchor and margins
}

void WaylandLayerShell::commit()
{
    if (m_layerSurface) {
        // Just commit the surface - configure ack is handled in configure callback
        wl_surface_commit(m_surface);
        wl_display_roundtrip(m_display);
    }
}

void WaylandLayerShell::handleConfigure(uint32_t serial, uint32_t width, uint32_t height)
{
    m_pendingSerial = serial;
    m_pendingSize = QSize(width, height);
    m_configured = true;
    
    qDebug() << "WaylandLayerShell: Received configure event - serial:" << serial << "size:" << width << "x" << height;
    
    // Now that we're using the Qt window's native surface for the layer shell,
    // the compositor will position the window automatically based on the layer shell properties.
    // We just need to resize the window to match the configured size.
    
    if (m_window && width > 0 && height > 0) {
        m_window->resize(width, height);
        
        // Position the window based on layer shell configuration
        // For bottom layer, position at bottom of screen
        if (m_layer == Layer::Bottom) {
            QScreen* screen = m_window->screen();
            if (screen) {
                QRect screenGeometry = screen->geometry();
                int y = screenGeometry.height() - height;
                
                // On Wayland, the compositor controls positioning
                // We need to work with the layer shell surface, not against it
                // The layer shell surface will be positioned by the compositor
                // We just need to ensure the Qt window is properly configured
                
                // Set the window size to match the layer shell surface
                m_window->resize(width, height);
                
                // Set window flags for proper layer shell behavior
                m_window->setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
                
                // For Qt5, we need to accept that the Qt window positioning is limited
                // The layer shell surface will be positioned by the compositor
                // This is a fundamental limitation of using separate surfaces on Wayland
                qDebug() << "WaylandLayerShell: Layer shell surface configured for bottom positioning";
                qDebug() << "WaylandLayerShell: Screen height:" << screenGeometry.height() 
                         << "Panel height:" << height << "Expected y position:" << y;
                qDebug() << "WaylandLayerShell: Qt window geometry:" << m_window->geometry();
                qDebug() << "WaylandLayerShell: Note: Qt5 positioning is limited on Wayland due to separate surfaces";
            }
        }
        
        qDebug() << "WaylandLayerShell: Configure event - size:" << width << "x" << height 
                 << "window geometry:" << m_window->geometry();
    }
    
    // Don't commit here to avoid infinite loop - just ack the configure
    if (m_layerSurface && m_pendingSerial > 0) {
        zwlr_layer_surface_v1_ack_configure(m_layerSurface, m_pendingSerial);
        wl_surface_commit(m_surface);
    }
    
    emit configureRequested(serial, width, height);
}

void WaylandLayerShell::handleClosed()
{
    emit closed();
}

void WaylandLayerShell::configureCallback(void* data, zwlr_layer_surface_v1* surface, 
                                          uint32_t serial, uint32_t width, uint32_t height)
{
    Q_UNUSED(surface);
    static_cast<WaylandLayerShell*>(data)->handleConfigure(serial, width, height);
}

void WaylandLayerShell::closedCallback(void* data, zwlr_layer_surface_v1* surface)
{
    Q_UNUSED(surface);
    static_cast<WaylandLayerShell*>(data)->handleClosed();
}

const zwlr_layer_surface_v1_listener WaylandLayerShell::s_layerSurfaceListener = {
    configureCallback,
    closedCallback
};

void WaylandLayerShell::handleRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
    Q_UNUSED(version);
    
    if (strcmp(interface, "wl_compositor") == 0) {
        m_compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4));
    } else if (strcmp(interface, "zwlr_layer_shell_v1") == 0) {
        m_layerShell = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 4));
    } else if (strcmp(interface, "wl_output") == 0) {
        // Use the first output we find
        if (!m_output) {
            m_output = static_cast<wl_output*>(
                wl_registry_bind(registry, name, &wl_output_interface, 4));
        }
    }
}

#include "moc_waylandlayershell.cpp"
