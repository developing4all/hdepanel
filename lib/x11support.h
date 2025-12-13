/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Copyright (C) 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

#ifndef X11SUPPORT_H
#define X11SUPPORT_H

#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtCore/QTimer>
#if QT_VERSION >= 0x050000
#include <QApplication>
#if QT_VERSION < 0x060000
#include <QX11Info>
#endif
#include <xcb/xcb.h>
#else                                                                              
#include <QtGui/QApplication>                                                      
#include <QtGui/QX11Info>                                                          
#include <QtGui/QImage>                                                            
#endif

// Avoid including X11 headers here to prevent macro collisions (e.g. None).
// Forward declare X11 types used in method signatures.
typedef union _XEvent XEvent;

class X11Support: public QObject
{
	Q_OBJECT
public:
	X11Support();
	~X11Support();
	static int detectTopPanelHeight(Display *dpy);

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
	static QVector<unsigned long> getWindowPropertyCardinalArray(unsigned long window, const QString& name);
	static QVector<unsigned long> getWindowPropertyWindowsArray(unsigned long window, const QString& name);
	static QVector<unsigned long> getWindowPropertyAtomsArray(unsigned long window, const QString& name);
    static QVector<unsigned long> getAllWindows();
    static void getAllWindowsRecursive(void* dpy, unsigned long window, QVector<unsigned long>& windows);
	static QString getWindowPropertyUTF8String(unsigned long window, const QString& name);
	static QString getWindowPropertyLatin1String(unsigned long window, const QString& name);
	static QString getWindowName(unsigned long window);
	static QIcon getWindowIcon(unsigned long window);
    static bool getWindowMinimizedState(unsigned long window);
	static bool getWindowUrgency(unsigned long window);

    // Select X11 input events for a window on the *same* X connection used by Qt/X11Support.
    // This is required for XCB_PROPERTY_NOTIFY (title/icon/urgency changes) to reach our event filter.
    static void selectInput(unsigned long window, long eventMask);

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
    static void setStrut(unsigned long _wid,
                  int left, int right,
                  int top,  int bottom,

                  int leftStartY,   int leftEndY,
                  int rightStartY,  int rightEndY,
                  int topStartX,    int topEndX,
                  int bottomStartX, int bottomEndX
                  );

private slots:
    void pollX11Events();

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

    // For Qt6 builds (and any case where Qt doesn't deliver X11 events for our masks),
    // we poll our own X11 connection to receive PropertyNotify/Configure/Destroy events.
    QTimer* m_x11PollTimer = nullptr;
};

#endif
