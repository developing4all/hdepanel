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

#ifndef X11SUPPORT_H
#define X11SUPPORT_H

#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#if QT_VERSION >= 0x050000                                                         
#include <QApplication>                                                            
#include <QX11Info>                                                                
#include <xcb/xcb.h>
#else                                                                              
#include <QtGui/QApplication>                                                      
#include <QtGui/QX11Info>                                                          
#include <QtGui/QImage>                                                            
#endif

// TODO: Keep all the X11 stuff with scary defines below normal headers.
#include <X11/Xlib.h>

class X11Support: public QObject
{
	Q_OBJECT
public:
	X11Support();
	~X11Support();

#if QT_VERSION >= 0x050000
    void onX11Event(xcb_generic_event_t *event);
#endif
	void onX11Event(XEvent* event);

	static X11Support* instance()
	{
		return m_instance;
	}

	static unsigned long rootWindow();
	static unsigned long atom(const QString& name);

	static void removeWindowProperty(unsigned long window, const QString& name);
	static void setWindowPropertyCardinalArray(unsigned long window, const QString& name, const QVector<unsigned long>& values);
	static void setWindowPropertyCardinal(unsigned long window, const QString& name, unsigned long value);
	static void setWindowPropertyVisualId(unsigned long window, const QString& name, unsigned long value);
	static unsigned long getWindowPropertyCardinal(unsigned long window, const QString& name);
	static unsigned long getWindowPropertyWindow(unsigned long window, const QString& name);
	static QVector<unsigned long> getWindowPropertyWindowsArray(unsigned long window, const QString& name);
	static QVector<unsigned long> getWindowPropertyAtomsArray(unsigned long window, const QString& name);
	static QString getWindowPropertyUTF8String(unsigned long window, const QString& name);
	static QString getWindowPropertyLatin1String(unsigned long window, const QString& name);
	static QString getWindowName(unsigned long window);
	static QIcon getWindowIcon(unsigned long window);
    static bool getWindowMinimizedState(unsigned long window);
	static bool getWindowUrgency(unsigned long window);
	static void registerForWindowPropertyChanges(unsigned long window);
    static void registerForWindowStructureNotify(unsigned long window);
	static void registerForTrayIconUpdates(unsigned long window);
	static void activateWindow(unsigned long window);
	static void minimizeWindow(unsigned long window);
	static void closeWindow(unsigned long window);
	static void destroyWindow(unsigned long window);
	static void killClient(unsigned long window);
	static bool makeSystemTray(unsigned long window);
	static void freeSystemTray();
	static unsigned long getARGBVisualId();
	static void redirectWindow(unsigned long window);
	static void unredirectWindow(unsigned long window);
	static QPixmap getWindowPixmap(unsigned long window);
    static QRect getWindowWindowsGeometry(unsigned long window);
	static void resizeWindow(unsigned long window, int width, int height);
	static void moveWindow(unsigned long window, int x, int y);
	static void mapWindow(unsigned long window);
	static void reparentWindow(unsigned long window, unsigned long parent);
	static void setWindowBackgroundBlack(unsigned long window);
    // See
    static void setStrut(Window _wid,
                  int left, int right,
                  int top,  int bottom,

                  int leftStartY,   int leftEndY,
                  int rightStartY,  int rightEndY,
                  int topStartX,    int topEndX,
                  int bottomStartX, int bottomEndX
                  );

signals:
	void windowClosed(unsigned long window);
	void windowReconfigured(unsigned long window, int x, int y, int width, int height);
	void windowDamaged(unsigned long window);
	void windowPropertyChanged(unsigned long window, unsigned long atom);
    void clientMessageReceived(unsigned long window, unsigned long atom, void* data);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

private:
	static unsigned long systemTrayAtom();

	static X11Support* m_instance;
	int m_damageEventBase;
	QMap<QString, unsigned long> m_cachedAtoms;
};

#endif
