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
 * version 3.0 of the License, or (at your option) any later version.
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

#include "layershellqtintegration.h"
#include <QDebug>
#include <QGuiApplication>
#include <QWindow>
#if QT_VERSION < 0x060000
#include <QX11Info>
#endif

#ifdef HDE_HAVE_LAYERSHELLQT
#include <LayerShellQt/shell.h>
#include <LayerShellQt/window.h>
#endif

LayerShellQtIntegration::LayerShellQtIntegration(QObject* parent)
    : QObject(parent)
    , m_available(false)
    , m_enabled(false)
{
#if QT_VERSION < 0x060000
    // LayerShellQt is not supported on Qt5 due to plugin loading issues
    // Users should use Qt6 for proper Wayland layer-shell support
    m_available = false;
    qWarning() << "";
    qWarning() << "==================================================================================";
    qWarning() << "NOTE: LayerShellQt requires Qt6 for proper Wayland layer-shell support.";
    qWarning() << "Please rebuild hdepanel with Qt6 for optimal Wayland experience with:";
    qWarning() << "  - Proper panel positioning (top/bottom)";
    qWarning() << "  - Screen space reservation (windows won't overlap panel)";
    qWarning() << "";
    qWarning() << "Current Qt5 build will use fallback positioning (limited functionality).";
    qWarning() << "==================================================================================";
    qWarning() << "";
#else
#ifdef HDE_HAVE_LAYERSHELLQT
    m_available = true;
    qDebug() << "LayerShellQtIntegration: LayerShellQt is available (Qt6 with system library)";
#else
    qDebug() << "LayerShellQtIntegration: LayerShellQt is not available (not compiled with support)";
#endif
#endif
}

LayerShellQtIntegration::~LayerShellQtIntegration()
{
}

bool LayerShellQtIntegration::isAvailable() const
{
    return m_available;
}

void LayerShellQtIntegration::enableLayerShell()
{
#ifdef HDE_HAVE_LAYERSHELLQT
    if (!m_available) {
        qDebug() << "LayerShellQtIntegration: Cannot enable - LayerShellQt not available";
        return;
    }

    if (m_enabled) {
        qDebug() << "LayerShellQtIntegration: Layer shell already enabled";
        return;
    }

    LayerShellQt::Shell::useLayerShell();
    m_enabled = true;
    qDebug() << "LayerShellQtIntegration: Layer shell enabled successfully";
#else
    qDebug() << "LayerShellQtIntegration: Cannot enable - LayerShellQt not compiled in";
#endif
}

void LayerShellQtIntegration::configureWindow(QWindow* window, 
                                             int layer, 
                                             int anchors, 
                                             int exclusiveZone,
                                             const QMargins& margins,
                                             int keyboardInteractivity)
{
#ifdef HDE_HAVE_LAYERSHELLQT
    if (!m_available || !window) {
        qDebug() << "LayerShellQtIntegration: Cannot configure - not available or invalid window";
        return;
    }

    if (!m_enabled) {
        qDebug() << "LayerShellQtIntegration: Layer shell not enabled, enabling now";
        enableLayerShell();
    }

    qDebug() << "LayerShellQtIntegration: Configuring window for layer shell";
    qDebug() << "LayerShellQtIntegration: Window handle:" << window->handle();
    qDebug() << "LayerShellQtIntegration: Window is visible:" << window->isVisible();

    LayerShellQt::Window *layerWindow = nullptr;
    try {
        layerWindow = LayerShellQt::Window::get(window);
        if (!layerWindow) {
            qDebug() << "LayerShellQtIntegration: Failed to get LayerShellQt::Window";
            emit errorOccurred("Failed to get LayerShellQt::Window");
            return;
        }
        
        qDebug() << "LayerShellQtIntegration: Got LayerShellQt::Window successfully";
    } catch (...) {
        qDebug() << "LayerShellQtIntegration: Exception occurred while getting LayerShellQt::Window";
        emit errorOccurred("Exception occurred while getting LayerShellQt::Window");
        return;
    }

    // Configure the layer shell properties with error handling
    try {
        qDebug() << "LayerShellQtIntegration: Setting layer to:" << layer;
        layerWindow->setLayer(static_cast<LayerShellQt::Window::Layer>(layer));
        
        qDebug() << "LayerShellQtIntegration: Setting anchors to:" << anchors;
        layerWindow->setAnchors(static_cast<LayerShellQt::Window::Anchors>(anchors));
        
        qDebug() << "LayerShellQtIntegration: Setting exclusive zone to:" << exclusiveZone;
        layerWindow->setExclusiveZone(exclusiveZone);
        
        qDebug() << "LayerShellQtIntegration: Setting margins to:" << margins;
        layerWindow->setMargins(margins);
        
        qDebug() << "LayerShellQtIntegration: Setting keyboard interactivity to:" << keyboardInteractivity;
        layerWindow->setKeyboardInteractivity(static_cast<LayerShellQt::Window::KeyboardInteractivity>(keyboardInteractivity));
    } catch (...) {
        qDebug() << "LayerShellQtIntegration: Exception occurred while configuring layer shell properties";
        emit errorOccurred("Exception occurred while configuring layer shell properties");
        return;
    }

    qDebug() << "LayerShellQtIntegration: Window configured successfully";
    qDebug() << "LayerShellQtIntegration: Layer:" << layer << "Anchors:" << anchors 
             << "ExclusiveZone:" << exclusiveZone << "Margins:" << margins;

    emit windowConfigured(window);
#else
    Q_UNUSED(window)
    Q_UNUSED(layer)
    Q_UNUSED(anchors)
    Q_UNUSED(exclusiveZone)
    Q_UNUSED(margins)
    Q_UNUSED(keyboardInteractivity)
    qDebug() << "LayerShellQtIntegration: Cannot configure - LayerShellQt not compiled in";
#endif
}

void LayerShellQtIntegration::setSize(QWindow* window, const QSize& size)
{
    if (!window) {
        return;
    }

    window->resize(size);
    qDebug() << "LayerShellQtIntegration: Set window size to:" << size;
}

bool LayerShellQtIntegration::isWayland()
{
#if QT_VERSION < 0x060000
    return !QX11Info::isPlatformX11();
#else
    return !qApp->platformName().toLower().contains("xcb");
#endif
}
