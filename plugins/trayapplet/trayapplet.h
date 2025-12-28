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
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef TRAYAPPLET_H
#define TRAYAPPLET_H

#include <QtCore/QVector>
#include <QtCore/QSize>
#include "applet.h"

class TrayItem;
class SniTrayItem;
class SniWatcher;

class TrayApplet: public Applet
{
	Q_OBJECT
public:
    TrayApplet(PanelWindow* panelWindow = 0);
	~TrayApplet();
    void close();
    virtual void setPanelWindow(PanelWindow* panelWindow);

	bool init();
    void startPlugin(){}

	QSize desiredSize();

	void registerTrayItem(TrayItem* trayItem);
	void unregisterTrayItem(TrayItem* trayItem);
	void registerSniTrayItem(SniTrayItem* trayItem);
	void unregisterSniTrayItem(SniTrayItem* trayItem);

	int iconSize() const { return m_iconSize; }

protected:
	void layoutChanged();

public slots:
    void fontChanged(){}

private slots:
    void clientMessageReceived(unsigned long window, unsigned long atom, void* data);
	void windowClosed(unsigned long window);
	void windowReconfigured(unsigned long window, int x, int y, int width, int height);
	void windowDamaged(unsigned long window);

private:
	void updateLayout();
	QString getX11TrayItemAppId(unsigned long window);
    QString getSniTrayItemAppId(class SniItemProxy* item) const;
	QString normalizeAppId(const QString& id);
	bool isSameApp(const QString& x11AppId, const QString& sniAppId);

	bool m_initialized;
	QVector<TrayItem*> m_trayItems;
	QVector<SniTrayItem*> m_sniTrayItems;
	int m_iconSize;
	int m_spacing;
    class SniWatcher* m_sniWatcher; // Wayland: DBus-based tray
    bool m_destroying = false;

    // Cache atoms / ids used in hot-path client message handling
    unsigned long m_trayOpcodeAtom = 0;
    unsigned long m_managerAtom = 0;
    unsigned long m_systemTrayAtom = 0;
    unsigned long m_trayWindowId = 0;

public:
    bool isDestroying() const { return m_destroying; }
};

#endif // TRAYAPPLET_H
