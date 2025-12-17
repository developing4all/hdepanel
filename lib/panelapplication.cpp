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

#include "panelapplication.h"
#include "settings.h"
#include "unifiediconservice.h"
#include "desktopdatastore.h"

#include <QAction>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QProcess>
#include <QFile>
#include <QDir>

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
}

PanelApplication::~PanelApplication()
{
    deletePanels();

    if (m_x11support)
        delete m_x11support;
    delete m_iconLoader;

    m_instance = NULL;
}

void PanelApplication::deletePanels()
{
    qDebug() << "Deleting Panels";

    // Stop all timers and clean up gracefully
    while(!m_panelWindows.isEmpty())
    {
        PanelWindow* panel = m_panelWindows.takeLast();
        if (panel) {
            // Hide the panel first to stop any ongoing operations
            panel->hide();
            delete panel;
        }
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
    QStringList panels = Settings::value( "", "panels", QStringList() ).toStringList();
    QString panel_id = "panel_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    panels << panel_id;
    Settings::setValue( "", "panels", panels );
    if (Settings::s_settings) Settings::s_settings->sync();

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

    // 1) Update settings first (so it's persisted even if UI deletion is deferred)
    QStringList panels = Settings::value("", "panels", QStringList()).toStringList();
    panels.removeAll(panel_id);
    Settings::setValue("", "panels", panels);
    if (Settings::s_settings) {
        Settings::s_settings->beginGroup(panel_id);
        Settings::s_settings->remove("");
        Settings::s_settings->endGroup();
        Settings::s_settings->sync();
    }

    // 2) Defer window deletion to avoid deleting during its own slot/context menu handling
    for (int i = 0; i < m_panelWindows.size(); ++i) {
        PanelWindow* panel = m_panelWindows[i];
        if (panel && panel->id() == panel_id) {
            panel->hide();
            m_panelWindows.removeAt(i);
            panel->deleteLater();
            break;
        }
    }
}


void PanelApplication::reinit()
{
    deletePanels();
	init();
}

void PanelApplication::init()
{
    QElapsedTimer initTimer;
    initTimer.start();
    
    // Start loading desktop entries once at application startup
    QElapsedTimer dataStoreTimer;
    dataStoreTimer.start();
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    if (dataStore) {
        dataStore->loadAllDesktopEntries();
    }
    
    // Try to detect system icon theme
    QElapsedTimer themeTimer;
    themeTimer.start();
    QString systemIconTheme = detectSystemIconTheme();
    if (!systemIconTheme.isEmpty()) {
        m_defaultIconThemeName = systemIconTheme;
        // Set Qt's icon theme to the detected directory name
        QIcon::setThemeName(m_defaultIconThemeName);
    }
    
    // This should be changed in any panel
    // Read the same key used by settings dialog
    setIconThemeName(Settings::value("Main", "iconThemeName", QVariant("default")).toString());

    QStringList panels = Settings::value("", "panels", QStringList() ).toStringList();

    if(panels.empty())
    {
        return addPanel(1);
        //panels = Settings::value("General", "panels", QStringList() ).toStringList();
    }
    else
    {
        //qDebug() << "panels: " << panels;
    }

    QElapsedTimer panelTimer;
    panelTimer.start();
    foreach (const QString &panel_id, panels) {
        showPanel(panel_id);
    }
}

void PanelApplication::showPanel(const QString& panel_id)
{
    PanelWindow* panelWindow = new PanelWindow(panel_id);
    // Use Normal layout policy (no longer using FillSpace)
    panelWindow->setLayoutPolicy(PanelWindow::Normal);
    panelWindow->setDockMode(true);
    
    // Update layout to ensure window has correct size
    panelWindow->updateLayout();
    
    // Show the window first so winId() is valid and geometry is established
    panelWindow->show();
    
    // Now position after the window is shown and has valid winId/size
    // Use a small delay to ensure the window is fully mapped
    QTimer::singleShot(10, [panelWindow]() {
        panelWindow->updatePosition();
    });
    
    m_panelWindows.append(panelWindow);
    //QObject::connect(this, SIGNAL(aboutToQuit()), panelWindow, SLOT(deleteLater()) );
}

void PanelApplication::setIconThemeName(const QString& iconThemeName)
{
    QString normalizedThemeName = iconThemeName;
    if (iconThemeName == "default") {
        normalizedThemeName = m_defaultIconThemeName;
    } else {
        // Normalize the theme name to its actual directory name
        QString actualDirName = findThemeDirectory(iconThemeName);
        if (!actualDirName.isEmpty()) {
            normalizedThemeName = actualDirName;
        } else {
            qWarning() << "Could not find directory for theme" << iconThemeName << ", falling back to default.";
            normalizedThemeName = m_defaultIconThemeName;
        }
    }

    m_iconThemeName = normalizedThemeName;
    QIcon::setThemeName(m_iconThemeName);

    // Clear unified icon service cache to force reload with new theme
    UnifiedIconService::instance()->setThemeName(m_iconThemeName);

    emit iconThemeChanged(m_iconThemeName); // Emit signal after theme change
}

QString PanelApplication::findThemeDirectory(const QString& themeName) const
{
    // Search for the actual directory name that corresponds to the theme name
    // The theme name might be the display name (from Name= in index.theme)
    // but we need the actual directory name (case-sensitive)
    foreach(const QString& themePath, QIcon::themeSearchPaths()) {
        QDir themeDir(themePath);
        foreach(const QFileInfo& item, themeDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString indexFile;
            QString mainSection;
            
            if (QFile::exists(item.absoluteFilePath() + "/index.desktop")) {
                indexFile = item.absoluteFilePath() + "/index.desktop";
                mainSection = "KDE Icon Theme";
            } else if (QFile::exists(item.absoluteFilePath() + "/index.theme")) {
                indexFile = item.absoluteFilePath() + "/index.theme";
                mainSection = "Icon Theme";
            }
            
            if (!indexFile.isEmpty()) {
                QSettings settings(indexFile, QSettings::IniFormat);
                settings.beginGroup(mainSection);
                QString name = settings.value("Name").toString();
                
                       // Check if the Name field matches what we're looking for
                       if (name.compare(themeName, Qt::CaseInsensitive) == 0) {
                           return item.fileName(); // Return the actual directory name
                       }
            }
        }
    }
    
    return QString();
}

QString PanelApplication::detectSystemIconTheme() const
{
    QString detectedTheme;
    
    // Try GTK settings first (most common on Wayland)
    QProcess gsettings;
    gsettings.start("gsettings", QStringList() << "get" << "org.gnome.desktop.interface" << "icon-theme");
    gsettings.waitForFinished(1000);
           if (gsettings.exitCode() == 0) {
               QString output = gsettings.readAllStandardOutput().trimmed();
               if (output.startsWith("'") && output.endsWith("'")) {
                   detectedTheme = output.mid(1, output.length() - 2);
               }
           }
    
    // Try KDE settings as fallback
    if (detectedTheme.isEmpty()) {
        QProcess kreadconfig;
        kreadconfig.start("kreadconfig5", QStringList() << "--file" << "kdeglobals" << "--group" << "Icons" << "--key" << "Theme");
        kreadconfig.waitForFinished(1000);
           if (kreadconfig.exitCode() == 0) {
               detectedTheme = kreadconfig.readAllStandardOutput().trimmed();
           }
    }
    
    // Check system default theme if no explicit theme is set
    if (detectedTheme.isEmpty()) {
        QSettings defaultTheme("/usr/share/icons/default/index.theme", QSettings::IniFormat);
        defaultTheme.beginGroup("Icon Theme");
        QString inherits = defaultTheme.value("Inherits").toString();
        if (!inherits.isEmpty()) {
            detectedTheme = inherits;
        }
    }
    
    // Convert theme name to actual directory name
    // This is necessary because the theme's display name (from Name= in index.theme)
    // might not match the actual directory name (case sensitive filesystem)
    if (!detectedTheme.isEmpty()) {
        QString actualDirName = findThemeDirectory(detectedTheme);
        if (!actualDirName.isEmpty()) {
            return actualDirName;
        }
    }
    
    // Verify the theme actually exists (fallback if findThemeDirectory didn't find it)
    if (!detectedTheme.isEmpty()) {
        foreach(const QString& themePath, QIcon::themeSearchPaths()) {
            QString themeDir = themePath + "/" + detectedTheme;
            if (QDir(themeDir).exists() && 
                (QFile::exists(themeDir + "/index.theme") || QFile::exists(themeDir + "/index.desktop"))) {
                return detectedTheme;
            }
        }
    }
    
    return QString();
}
