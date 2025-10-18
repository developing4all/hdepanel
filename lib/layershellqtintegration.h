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

#ifndef LAYERSHELLQTINTEGRATION_H
#define LAYERSHELLQTINTEGRATION_H

#include <QObject>
#include <QWindow>
#include <QSize>

#ifdef HDE_HAVE_LAYERSHELLQT
#include <LayerShellQt/shell.h>
#include <LayerShellQt/window.h>
#endif

/**
 * @brief Integration with KDE's LayerShellQt for proper Wayland layer shell support
 */
class LayerShellQtIntegration : public QObject
{
    Q_OBJECT

public:
    explicit LayerShellQtIntegration(QObject* parent = nullptr);
    ~LayerShellQtIntegration();

    /**
     * @brief Check if LayerShellQt is available
     * @return true if available, false otherwise
     */
    bool isAvailable() const;

    /**
     * @brief Enable layer shell for the application
     * Must be called before creating any windows
     */
    void enableLayerShell();

    /**
     * @brief Configure a window for layer shell
     * @param window The QWindow to configure
     * @param layer The layer to use (background, bottom, top, overlay)
     * @param anchors Which edges to anchor to
     * @param exclusiveZone Exclusive zone size (0 for auto)
     * @param margins Margins from screen edges
     * @param keyboardInteractivity Keyboard interaction mode
     */
    void configureWindow(QWindow* window, 
                        int layer = 1, // LayerBottom
                        int anchors = 2 | 4 | 8, // AnchorBottom | AnchorLeft | AnchorRight
                        int exclusiveZone = 48,
                        const QMargins& margins = QMargins(0, 0, 0, 0),
                        int keyboardInteractivity = 0);

    /**
     * @brief Set the size for the layer shell window
     * @param window The QWindow
     * @param size The desired size
     */
    void setSize(QWindow* window, const QSize& size);

    /**
     * @brief Check if we're running on Wayland (works for both Qt5 and Qt6)
     * @return true if on Wayland, false otherwise
     */
    static bool isWayland();

signals:
    void windowConfigured(QWindow* window);
    void errorOccurred(const QString& error);

private:
    bool m_available;
    bool m_enabled;
};

#endif // LAYERSHELLQTINTEGRATION_H
