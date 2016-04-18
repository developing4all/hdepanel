/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
 * Copyright (C) 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

#ifndef APPLICATIONSMENUAPPLET_H
#define APPLICATIONSMENUAPPLET_H
#include <QtCore/QList>
#include <QtCore/QMap>
#if QT_VERSION >= 0x050000
#include <QAction>
#include <QStyleFactory>
#else
#include <QtGui/QAction>
#include <QtGui/QPlastiqueStyle>
#endif
#include "../../lib/applet.h"

#if QT_VERSION < 0x050000
class ApplicationsMenuStyle: public QPlastiqueStyle
{
    Q_OBJECT
public:
    int pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const;
};
#endif

class SubMenu
{
public:
    SubMenu()
    {
    }

    SubMenu(QMenu* parent, const QString& title, const QString& category, const QString& icon);

    QMenu* menu()
    {
        return m_menu;
    }

    const QString& category() const
    {
        return m_category;
    }

private:
    QMenu* m_menu;
    QString m_category;
};

class TextGraphicsItem;
class DesktopApplication;

class ApplicationsMenuApplet: public Applet
{
    Q_OBJECT
public:
    ApplicationsMenuApplet(PanelWindow* panelWindow =0);
    ~ApplicationsMenuApplet();
    virtual void setPanelWindow(PanelWindow* panelWindow);

    bool init();
    void close(){}

    QSize desiredSize();
    void clicked();

protected:
    void layoutChanged();
    bool isHighlighted();

private slots:
    void actionTriggered();
    void applicationUpdated(const DesktopApplication& app);
    void applicationRemoved(const QString& path);

public slots:
    void fontChanged();

private:
#if QT_VERSION < 0x050000
    ApplicationsMenuStyle m_style;
#endif
    TextGraphicsItem* m_textItem;
    bool m_menuOpened;
    QMenu* m_menu;
    QList<SubMenu> m_subMenus;
    QMap<QString, QAction*> m_actions;
};

class ApplicationsMenuAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)
public:
    Applet* createApplet(PanelWindow* panelWindow);
};


#endif
