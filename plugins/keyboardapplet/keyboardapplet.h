/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
 * Authors:
 *   Haydar Alkaduhimi <haydar@hosting4all.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef KEYBOARDAPPLET_H
#define KEYBOARDAPPLET_H
#include "../../lib/applet.h"
#include <QtCore>
#include <QtDebug>

class TextGraphicsItem;
class TestApplet;
class PanelWindow;
class KeyboardApplet;
class Keyboard;
class QxtGlobalShortcut;

/**
 * The plugin class for the KeyboardApplet
 */
class KeyboardAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    KeyboardAppletPlugin(){}
    ~KeyboardAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) Q_DECL_OVERRIDE;
};

/**
 * @brief The KeyboardApplet class
 */
class KeyboardApplet: public Applet
{
    Q_OBJECT
public:
    /**
     * @brief Constructor
     * @param panelWindow
     */
    KeyboardApplet(PanelWindow* panelWindow = 0);
    ~KeyboardApplet();

    void close(){}
    virtual void setPanelWindow(PanelWindow* panelWindow);

    bool init();

    QSize desiredSize();

public slots:
    /**
     * @brief change keyboard layout whenever clicked
     */
    void clicked();

    /**
     * @brief change the keyboard layout forward
     */
    void changeForward();

    /**
     * @brief change the keyboard layout backward
     */
    void changeBackword();

    /**
     * @brief set current keyboard layout to the given layout
     * @param layout
     */
    void setCurrentLayout(QString layout);

    void fontChanged(){}

    /**
     * @brief show keyboard layout configuration dialog
     */
    void showConfigurationDialog();

protected:
    /**
     * @brief panel layout changed
     */
    void layoutChanged();

    bool isHighlighted(){ return false;}

    /**
     * @brief show a custom context menu
     * @param event
     */
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);

private:
    /**
     * @brief draw the current layout inside an image
     * @param layout
     */
    void drawCurrentLayout(QString layout = QString());

    QString m_current_layout;
    QStringList m_supported_layouts;

    QxtGlobalShortcut *backwardShortcut;
    QxtGlobalShortcut *forwardShortcut;

    TextGraphicsItem* m_textItem;

};

#endif // KEYBOARDAPPLET_H
