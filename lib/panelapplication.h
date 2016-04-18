/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * This Files has been imported to hde from qtpanel
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

#ifndef PANELAPPLICATION_H
#define PANELAPPLICATION_H

#include <QtCore/QSettings>                                                        
#include <QtCore/QTimer>
#if QT_VERSION >= 0x050000
#include <QApplication>
#include <QFont>
#include <QAbstractNativeEventFilter>
#include <xcb/xcb.h>
#else
#include <QtGui/QApplication>
#include <QtGui/QFont>
#endif
#include <QDebug>

#include "iconloader.h"                                                            
#include "dpisupport.h"                                                            
#include "desktopapplications.h"                                                   
//#include "ui_panelapplicationsettings.h"
#include "panelwindow.h"
#include "x11support.h"

class IconLoader;
class X11Support;
class DesktopApplications;

#if QT_VERSION >= 0x050000                                                         
class MyXcbEventFilter : public QAbstractNativeEventFilter                         
{                                                                                  
public:                   
    MyXcbEventFilter() :m_x11support(NULL) {}

    virtual bool nativeEventFilter(const QByteArray &/*eventType*/, void *message, long *) Q_DECL_OVERRIDE
    {                                                                              
        xcb_generic_event_t *ev = static_cast<xcb_generic_event_t *>(message);
        if (m_x11support) m_x11support->onX11Event(ev); 
        return false;                                                              
    }

    void setX11Support(X11Support *x11support) { m_x11support = x11support; }

private:
    X11Support *m_x11support;    
};                                                                                 
#endif

class PanelApplication: public QApplication 
{
	Q_OBJECT
public:
	PanelApplication(int& argc, char** argv);
	~PanelApplication();

	static PanelApplication* instance()
	{
		return m_instance;
	}

#if QT_VERSION >= 0x050000
    MyXcbEventFilter myXEv;
#endif

	bool x11EventFilter(XEvent* event);

	void init();

	void setFontName(const QString& fontName);
	void setIconThemeName(const QString& iconThemeName);

	const QFont& panelFont() const
	{
		return m_panelFont;
	}

public slots:
	void showConfigurationDialog();
    void addPanel(int standard = 0);
    void removePanel(const QString panel_id);
    void deletePanels();

private slots:
	void reinit();

private:
	static PanelApplication* m_instance;
	IconLoader* m_iconLoader;
	X11Support* m_x11support;
	DesktopApplications* m_desktopApplications;

	QString m_fontName;
	QString m_iconThemeName;
	PanelWindow::Anchor m_verticalAnchor;
	QString m_defaultIconThemeName;
	QFont m_panelFont;
	QVector<PanelWindow*> m_panelWindows;

    void showPanel(const QString& panel_id);
};

#endif
