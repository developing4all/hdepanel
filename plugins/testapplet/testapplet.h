/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
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
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef TESTAPPLET_H
#define TESTAPPLET_H

#include "../../lib/applet.h"
#include <QtCore>
#include <QtDebug>

class TextGraphicsItem;
class TestApplet;
class PanelWindow;

/**
 * The plugin class for the TestApplet
 */

class TestAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    TestAppletPlugin(){}
    ~TestAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) Q_DECL_OVERRIDE;
    QString name() const override { return "Test Applet"; }
};

/**
 * @brief A Test Applet that can be used as a base for new applets
 */
class TestApplet: public Applet
{
    Q_OBJECT
public:
    TestApplet(PanelWindow* panelWindow = 0);
    ~TestApplet(){}
    void close(){}
    virtual void setPanelWindow(PanelWindow* panelWindow);

    bool init(){return true;}
    //void startPlugin();

    QSize desiredSize();

public slots:
    void clicked(){}
    void fontChanged(){}

protected:
    void layoutChanged();
    bool isHighlighted(){ return false;}

private:
    TextGraphicsItem* m_textItem;

};

#endif // TESTAPPLET_H
