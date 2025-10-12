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
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "panelapplication.h"
#include "settings.h"

#include <QAction>
#include <QDateTime>
#include <QTimer>

PanelApplication* PanelApplication::m_instance = NULL;

PanelApplication::PanelApplication(int& argc, char** argv)
	: QApplication(argc, argv)
{
	m_instance = this;

	m_defaultIconThemeName = QIcon::themeName();
    if(m_defaultIconThemeName.isEmpty())
    {
        m_defaultIconThemeName = "oxygen";
    }

    setOrganizationName("developing4all");
    setApplicationName("hde/panel");

    Settings *settings = new Settings();
    Q_UNUSED(settings)

	m_iconLoader = new IconLoader();
	m_x11support = nullptr;
#if QT_VERSION >= 0x050000
    // Only initialize X11 support on X11 platform
#if QT_VERSION < 0x060000
    if (QX11Info::isPlatformX11()) {
#else
    if (qApp->platformName().toLower().contains("xcb")) {
#endif
        m_x11support = new X11Support();
        myXEv.setX11Support(m_x11support);
        installNativeEventFilter(&myXEv);
        installEventFilter(m_x11support);
    }
#else
    m_x11support = new X11Support();
    installEventFilter(m_x11support);
#endif
    m_desktopApplications = new DesktopApplications();

    //QObject::connect(this, SIGNAL(aboutToQuit()), this, SLOT(deletePanels()) );
}

PanelApplication::~PanelApplication()
{
    deletePanels();
    delete m_desktopApplications;
    if (m_x11support)
        delete m_x11support;
    delete m_iconLoader;

    m_instance = NULL;
}

void PanelApplication::deletePanels()
{
    qDebug() << "Deleting Panels";

    while(!m_panelWindows.isEmpty())
    {
        delete m_panelWindows.takeLast();
    }
    m_panelWindows.clear();
}




bool PanelApplication::x11EventFilter(XEvent* event)
{
    m_x11support->onX11Event(event);
	return false;
}

void PanelApplication::addPanel(int standard)
{
    QStringList panels = Settings::value( "panels", QStringList() ).toStringList();
    QString panel_id = "panel_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    panels << panel_id;

    Settings::setValue( "panels", panels );

    if(standard > 0)
    {
        // Add Standard items to the panel
        QStringList applets;
        applets << "StartApplet_" + QString::number(QDateTime::currentMSecsSinceEpoch())
                << "DockApplet_"  + QString::number(QDateTime::currentMSecsSinceEpoch())
                << "TrayApplet_" + QString::number(QDateTime::currentMSecsSinceEpoch())
                << "ClockApplet_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        Settings::setValue(panel_id, "applets", applets );
    }
    showPanel(panel_id);
}

void PanelApplication::removePanel(const QString panel_id)
{
    qDebug() << "Removing panel: " << panel_id;
}


void PanelApplication::reinit()
{
    deletePanels();
	init();
}

void PanelApplication::init()
{
#if QT_VERSION >= 0x050000
    installNativeEventFilter(&myXEv);
#endif
    // This should be changed in any panel
    setIconThemeName(Settings::value("iconThemeName", QVariant("default")).toString());

    QStringList panels = Settings::value("panels", QStringList() ).toStringList();

    if(panels.empty())
    {
        return addPanel(1);
        //panels = Settings::value("General", "panels", QStringList() ).toStringList();
    }
    else
    {
        //qDebug() << "panels: " << panels;
    }

    foreach (const QString &panel_id, panels) {
        showPanel(panel_id);
    }

}

void PanelApplication::showPanel(const QString& panel_id)
{
    PanelWindow* panelWindow = new PanelWindow(panel_id);
    // Use FillSpace to span full screen width
    panelWindow->setLayoutPolicy(PanelWindow::FillSpace);
    panelWindow->setDockMode(true);
    
    // Initialize applets first
    panelWindow->init();
    
    // Then update layout
    panelWindow->updateLayout();
    
    // Set position BEFORE showing the window
    panelWindow->updatePosition();
    
    panelWindow->show();
    
    // Update position after window is shown and layer surface is configured
    panelWindow->updatePosition();
    
    // Force position multiple times to ensure it sticks
    QTimer::singleShot(100, [panelWindow]() {
        panelWindow->updatePosition();
    });
    
    QTimer::singleShot(500, [panelWindow]() {
        panelWindow->updatePosition();
    });
    
    QTimer::singleShot(1000, [panelWindow]() {
        panelWindow->updatePosition();
    });
    
    
    m_panelWindows.append(panelWindow);
    //QObject::connect(this, SIGNAL(aboutToQuit()), panelWindow, SLOT(deleteLater()) );
}

void PanelApplication::setIconThemeName(const QString& iconThemeName)
{
	m_iconThemeName = iconThemeName;
	if(m_iconThemeName != "default")
		QIcon::setThemeName(m_iconThemeName);
	else
		QIcon::setThemeName(m_defaultIconThemeName);
}
