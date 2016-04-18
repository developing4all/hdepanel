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

#include "panelapplication.h"
#include "settings.h"

#include <QAction>
#include <QDateTime>

PanelApplication* PanelApplication::m_instance = NULL;

PanelApplication::PanelApplication(int& argc, char** argv)
	: QApplication(argc, argv)
{
	m_instance = this;

	m_defaultIconThemeName = QIcon::themeName();

    setOrganizationName("dev4all");
    setApplicationName("hde/panel");

    Settings *settings = new Settings();

    //QAction::setIconVisibleInMenu(true);

	m_iconLoader = new IconLoader();
	m_x11support = new X11Support();
#if QT_VERSION >= 0x050000
	myXEv.setX11Support(m_x11support);
#endif
    m_desktopApplications = new DesktopApplications();

    //QObject::connect(this, SIGNAL(aboutToQuit()), this, SLOT(deletePanels()) );
}

PanelApplication::~PanelApplication()
{
    deletePanels();
    delete m_desktopApplications;
    delete m_x11support;
    delete m_iconLoader;

    m_instance = NULL;
}

void PanelApplication::deletePanels()
{
    qDebug() << "Deleting Panels";
    /*
    for(int i = 0; i < m_panelWindows.size(); i++)
    {
        delete m_panelWindows[i];
    }
    m_panelWindows.clear();
    */
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

void PanelApplication::showConfigurationDialog()
{
    /*
	QDialog dialog;
	Ui::PanelApplicationSettings settingsUi;
	settingsUi.setupUi(&dialog);
	settingsUi.fontName->setText(m_fontName);
	settingsUi.iconThemeName->setText(m_iconThemeName);
	settingsUi.verticalPosition->setCurrentIndex(m_verticalAnchor == PanelWindow::Max ? 1 : 0);
	if(dialog.exec() == QDialog::Accepted)
	{
		QSettings settings;
		settings.setValue("fontName", settingsUi.fontName->text());
		settings.setValue("iconThemeName", settingsUi.iconThemeName->text());
		settings.setValue("verticalPosition", settingsUi.verticalPosition->currentText());

		// Don't want to delete objects right now (because we're called from those objects), schedule it for later.
		QTimer::singleShot(1, this, SLOT(reinit()));
	}
    */
}

void PanelApplication::addPanel(int standard)
{
    QStringList panels = Settings::value("General", "panels", QStringList() ).toStringList();
    QString panel_id = "panel_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    panels << panel_id;

    Settings::setValue("General", "panels", panels );

    if(standard > 0)
    {
        // Add Standard items to the panel
        QStringList applets;
        applets <<"ApplicationsMenuApplet" << "DockApplet" << "TrayApplet" << "ClockApplet";
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
    setIconThemeName(Settings::value("General", "iconThemeName", "default").toString());

    QStringList panels = Settings::value("General", "panels", QStringList() ).toStringList();

    if(panels.empty())
    {
        return addPanel(1);
        //panels = Settings::value("General", "panels", QStringList() ).toStringList();
    }
    else
    {
        qDebug() << "panels: " << panels;
    }

    foreach (const QString &panel_id, panels) {
        showPanel(panel_id);
    }

}

void PanelApplication::showPanel(const QString& panel_id)
{
    PanelWindow* panelWindow = new PanelWindow(panel_id);
    panelWindow->resize(adjustHardcodedPixelSize(128), adjustHardcodedPixelSize(32));
    panelWindow->setLayoutPolicy(PanelWindow::FillSpace);
    panelWindow->setDockMode(true);
    panelWindow->init();
    panelWindow->show();
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
