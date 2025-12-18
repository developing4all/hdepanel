/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Copyright: 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef DOCKAPPLETPLUGIN_H
#define DOCKAPPLETPLUGIN_H

#include <QtCore/QObject>
#include "applet.h"

// Forward declarations
class TaskBarApplet;

class TaskBarAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    TaskBarAppletPlugin();
    ~TaskBarAppletPlugin();

    Applet* createApplet(PanelWindow* panelWindow);
    QString name() const override { return tr("Task Bar"); }
};

#endif
