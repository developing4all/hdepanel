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

#include "desktopapplications.h"
#include "desktopdatastore.h"

#include <QtCore/QTimer>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QMutexLocker>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>
#include <climits>
#include <QtCore/QProcess>
#include <QtCore/QLocale>
#include <QtCore/QCoreApplication>
#include <QtGui/QIcon>
#include <QtGui/QGuiApplication>
#include "unifiediconservice.h"
#include "dpisupport.h"

bool DesktopApplication::init(const QString& path)
{
	m_path = path;

	QFile file(m_path);

	if(!file.exists())
		return false;

	QFileInfo fileInfo(file);
	m_lastUpdated = fileInfo.lastModified();

	if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;

	// Get current locale for translated names
	QString locale = QLocale::system().name(); // e.g., "nl_NL", "ar_SA"
	QString language = locale.split('_').first(); // e.g., "nl", "ar"
	
	// Keys to look for in order of preference
	QString localizedNameKey = "Name[" + locale + "]";     // Name[nl_NL]
	QString languageNameKey = "Name[" + language + "]";    // Name[nl]
	
	QString name;
	QString localizedName;
	
	QTextStream in(&file);
	while(!in.atEnd())
	{
		QString line = in.readLine();
		if(line[0] == '[')
		{
			if(line.contains("Desktop Entry"))
				continue;
			else
				break; // We only process "Desktop Entry" here.
		}
		if(line[0] == '#')
			continue;
		QStringList list = line.split('=');
		if(list.size() < 2)
			continue;
		QString key = list[0];
		QString value = list[1];
		if(key == "NoDisplay" && value == "true")
			m_isNoDisplay = true;
		if(key == "Name")
			name = value; // Fallback name
		// Check for exact locale match (e.g., Name[nl_NL])
		if(key == localizedNameKey)
			localizedName = value;
		// Check for language-only match (e.g., Name[nl])
		if(key == languageNameKey && localizedName.isEmpty())
			localizedName = value;
		if(key == "Exec")
			m_exec = value;
		if(key == "Icon")
		{
			m_iconName = value;
		}
        if(key == "Categories")
        {
            QStringList categories = value.split(';');
            m_categories.clear();
            for (const QString& category : categories) {
                QString trimmed = category.trimmed();
                if (!trimmed.isEmpty()) {
                    m_categories.append(trimmed);
                }
            }
        }
	}
	
	// Use localized name if available, otherwise fall back to default name
	m_name = localizedName.isEmpty() ? name : localizedName;

	return true;
}

bool DesktopApplication::exists() const
{
	return QFile(m_path).exists();
}

bool DesktopApplication::needUpdate() const
{
	return m_lastUpdated != QFileInfo(m_path).lastModified();
}

void DesktopApplication::launch() const
{
	QString exec = m_exec;

	// Handle special arguments.
	for(;;)
	{
		int argPos = exec.indexOf('%');
		if(argPos == -1)
			break;
		// For now, just remove them.
		int spacePos = exec.indexOf(' ', argPos);
		if(spacePos == -1)
			exec.resize(argPos);
		else
			exec.remove(argPos, spacePos - argPos);
	}

	exec = exec.trimmed();
	QStringList args = exec.split(' ');
	QString process = args[0];
	args.removeAt(0);
	QProcess::startDetached(process, args, getenv("HOME"));
}

DesktopApplications* DesktopApplications::m_instance = NULL;

// Helper function to convert DesktopEntryData to DesktopApplication
DesktopApplication DesktopApplications::convertFromDesktopEntryData(const DesktopEntryData& entryData)
{
    DesktopApplication app;
    
    // Set basic properties
    app.m_path = entryData.desktopFile;
    app.m_name = entryData.getDisplayName();
    app.m_iconName = entryData.icon;
    app.m_categories = entryData.categories;
    app.m_isNoDisplay = entryData.noDisplay || entryData.hidden;
    
    // Set exec from entryData
    app.m_exec = entryData.exec;
    
    // Load icon image on-demand if we have a QGuiApplication
    if (!entryData.icon.isEmpty() && qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        QImage iconImage = UnifiedIconService::instance()->loadIconAsImage(entryData.icon, 32);
        if (!iconImage.isNull()) {
            app.m_iconImage = iconImage;
        }
    }
    
    return app;
}

DesktopApplications::DesktopApplications()
	: m_abortWorker(false)
{
	m_instance = this;

	qRegisterMetaType<DesktopApplication>();

	m_updateTimer = new QTimer();
	m_updateTimer->setSingleShot(true);
	connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(refresh()));

	m_watcher = new QFileSystemWatcher();
	connect(m_watcher, SIGNAL(directoryChanged(QString)), this, SLOT(directoryChanged(QString)));
	connect(m_watcher, SIGNAL(fileChanged(QString)), this, SLOT(fileChanged(QString)));

	// Start worker and trigger initial update.
	start(QThread::IdlePriority);
	refresh();
}

DesktopApplications::~DesktopApplications()
{
	m_abortWorker = true;
	m_tasksWaitCondition.wakeAll(); // Wake all waiting threads
	
	// Wait for thread to finish with timeout
	if (!wait(5000)) // 5 second timeout
	{
		terminate();
		wait(1000); // Give it 1 more second to terminate gracefully
	}

	delete m_watcher;
	delete m_updateTimer;

	m_instance = NULL;
}

QList<DesktopApplication> DesktopApplications::applications()
{
	// Get applications from DesktopDataStore instead of internal m_applications
	DesktopDataStore* dataStore = DesktopDataStore::instance();
	if (!dataStore) {
		qDebug() << "DesktopApplications::applications() - DataStore is null, returning empty list";
		return QList<DesktopApplication>();
	}
	
	// Get currently loaded desktop entries (non-blocking)
	QList<DesktopEntryData> entries = dataStore->getAllDesktopEntries();
	
	// Convert DesktopEntryData to DesktopApplication
	QList<DesktopApplication> apps;
	foreach (const DesktopEntryData& entry, entries) {
		// Only include Application type entries that should be shown
		if (entry.type == "Application" && entry.shouldShow() && entry.isValid) {
			apps.append(convertFromDesktopEntryData(entry));
		}
	}
	
	return apps;
}

DesktopApplication DesktopApplications::applicationFromPath(const QString& path)
{
	// Get application from DesktopDataStore instead of internal m_applications
	DesktopDataStore* dataStore = DesktopDataStore::instance();
	if (!dataStore) {
		return DesktopApplication();
	}
	
	// Get the specific desktop entry
	DesktopEntryData entry = dataStore->getDesktopEntry(path);
	if (entry.isValid && entry.type == "Application") {
		return convertFromDesktopEntryData(entry);
	}
	
	return DesktopApplication();
}

void DesktopApplications::launch(const QString& path)
{
	// Get the application from DesktopDataStore (consistent with applications() and applicationFromPath())
	DesktopApplication app = applicationFromPath(path);
	if (!app.path().isEmpty()) {
		app.launch();
	} else {
		qDebug() << "DesktopApplications::launch() - Application not found for path:" << path;
	}
}

void DesktopApplications::run()
{
	while (!m_abortWorker)
	{
		// Extract next task.
		bool isImageTask = false;
		QString path;
		{
			QMutexLocker lock(&m_tasksMutex);
			
			// Wait for tasks with timeout to prevent infinite blocking
			if(m_fileTasks.isEmpty() && m_imageTasks.isEmpty())
			{
				// Wait for up to 1 second, then check abort condition
				if (!m_tasksWaitCondition.wait(&m_tasksMutex, 1000))
				{
					// Timeout occurred, check if we should abort
					if (m_abortWorker)
						return;
					continue; // Try again
				}
			}
			
			// Double-check abort condition after waiting
			if(m_abortWorker)
				return;
				
			if(!m_fileTasks.isEmpty())
			{
				path = m_fileTasks.first();
				m_fileTasks.removeFirst();
			}
			else if(!m_imageTasks.isEmpty())
			{
				isImageTask = true;
				path = m_imageTasks.first();
				m_imageTasks.removeFirst();
			}
			else
			{
				// No tasks available, continue to next iteration
				continue;
			}
		}

		// Additional safety check before processing
		if (m_abortWorker)
			return;

		if(!isImageTask)
		{
			// File task.
			m_applicationsMutex.lock();
			bool needUpdate = (!m_applications.contains(path)) || (m_applications[path].needUpdate());
			m_applicationsMutex.unlock();

			if(needUpdate)
			{
				DesktopApplication app;
				if(app.init(path))
				{
					m_applicationsMutex.lock();
					m_applications[path] = app;
					emit applicationUpdated(app);
					m_applicationsMutex.unlock();

					m_tasksMutex.lock();
					if(!m_imageTasks.contains(path))
						m_imageTasks.append(path);
					m_tasksMutex.unlock();
				}
			}
		}
		else
		{
			// Additional safety check before image processing
			if (m_abortWorker)
				return;
				
			// Image task.
			m_applicationsMutex.lock();
			QString iconName;
			if(m_applications.contains(path))
			{
				iconName = m_applications[path].iconName();
			}
			m_applicationsMutex.unlock();

			if(!iconName.isEmpty())
			{
				// Use unified icon service for consistent icon loading
				QImage iconImage = UnifiedIconService::instance()->loadIconAsImage(iconName, adjustHardcodedPixelSize(32));

				m_applicationsMutex.lock();
				if(m_applications.contains(path))
					m_applications[path].setIconImage(iconImage);
				emit applicationUpdated(m_applications[path]);
				m_applicationsMutex.unlock();
			}
		}
	}
}

void DesktopApplications::directoryChanged(const QString& path)
{
    Q_UNUSED(path)

	m_updateTimer->stop();
	m_updateTimer->start();
}

void DesktopApplications::fileChanged(const QString& path)
{
	QMutexLocker lock(&m_tasksMutex);
	if(!m_fileTasks.contains(path))
		m_fileTasks.append(path);
}

void DesktopApplications::refresh()
{
	m_tasksMutex.lock();

	m_fileTasks.clear();
	m_imageTasks.clear();

	if(!m_watcher->directories().isEmpty())
		m_watcher->removePaths(m_watcher->directories());
	if(!m_watcher->files().isEmpty())
		m_watcher->removePaths(m_watcher->files());

	QString xdgDataDirs;
	char* xdgDataDirsEnv = getenv("XDG_DATA_DIRS");
	if(xdgDataDirsEnv != NULL)
		xdgDataDirs = xdgDataDirsEnv;
	else
		xdgDataDirs = "/usr/local/share/:/usr/share/";

	QStringList dirs = xdgDataDirs.split(':');

	foreach(const QString& path, dirs)
	{
		QDir dir(path);
		QString appsPath = dir.absoluteFilePath("applications");
		if(dir.exists())
			traverse(QDir(appsPath));
	}

		QStringList removeList;
		foreach(const DesktopApplication& app, m_applications)
		{
			if(!app.exists())
				removeList.append(app.path());
		}

		foreach(const QString& path, removeList)
		{
			m_applications.remove(path);
			emit applicationRemoved(path);
		}

	m_tasksMutex.unlock();
	m_tasksWaitCondition.wakeOne();
}

void DesktopApplications::refreshApplications()
{
	refresh();
}

void DesktopApplications::traverse(const QDir& dir)
{
	if(!dir.exists())
		return;

	m_watcher->addPath(dir.canonicalPath());

	QFileInfoList fileInfos = dir.entryInfoList(QStringList("*.desktop"), QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);
	foreach(const QFileInfo& fileInfo, fileInfos)
	{
		if(fileInfo.isDir())
		{
			traverse(fileInfo.canonicalFilePath());
		}
		else
		{
			m_watcher->addPath(fileInfo.canonicalFilePath());
			m_fileTasks.append(fileInfo.canonicalFilePath());
		}
	}
}

QString DesktopApplications::getApplicationIcon(const QString& appId, const QString& wmClass)
{
    if (appId.isEmpty() && wmClass.isEmpty()) {
        return "application-x-executable";
    }
    
    // Search for matching applications
    QList<DesktopApplication> matches = searchApplications(appId, wmClass);
    
    if (!matches.isEmpty()) {
        // Return the icon from the first match
        QString icon = matches.first().iconName();
        if (!icon.isEmpty()) {
            return icon;
        }
    }
    
    // Fallback: use the appId or wmClass itself as the icon name
    QString fallbackIcon = !appId.isEmpty() ? appId.toLower() : wmClass.toLower();
    return fallbackIcon.isEmpty() ? "application-x-executable" : fallbackIcon;
}

QList<DesktopApplication> DesktopApplications::searchApplications(const QString& appId, const QString& wmClass)
{
    QList<DesktopApplication> results;
    
    if (appId.isEmpty() && wmClass.isEmpty()) {
        return results;
    }
    
    QString appIdLower = appId.toLower();
    QString wmClassLower = wmClass.toLower();
    
    QList<DesktopApplication> apps = applications();
    
    for (const DesktopApplication& app : apps) {
        QString fileName = QFileInfo(app.path()).completeBaseName().toLower();
        QString appName = app.name().toLower();
        
        bool matches = false;
        
        // Check appId matches
        if (!appIdLower.isEmpty()) {
            if (fileName == appIdLower || 
                fileName.startsWith(appIdLower + "-") || 
                fileName.startsWith(appIdLower + "_") ||
                appName == appIdLower ||
                fileName.contains(appIdLower) ||
                appName.contains(appIdLower)) {
                matches = true;
            }
        }
        
        // Check wmClass matches
        if (!wmClassLower.isEmpty()) {
            if (fileName == wmClassLower || 
                fileName.startsWith(wmClassLower + "-") || 
                fileName.startsWith(wmClassLower + "_") ||
                appName == wmClassLower ||
                fileName.contains(wmClassLower) ||
                appName.contains(wmClassLower)) {
                matches = true;
            }
        }
        
        if (matches) {
            results.append(app);
        }
    }
    
    return results;
}