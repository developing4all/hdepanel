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

#ifndef DESKTOPAPPLICATIONS_H
#define DESKTOPAPPLICATIONS_H

#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>
#include <QtCore/QMap>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>
#include <QtGui/QImage>

class DesktopApplication
{
public:
	DesktopApplication()
		: m_isNoDisplay(false)
	{
	}

	bool init(const QString& path);
    void startPlugin(){}


	bool exists() const;

	bool needUpdate() const;

	void launch() const;

    bool isNoDisplay() const { return m_isNoDisplay; }
	const QString& path() const { return m_path; }
	const QString& name() const { return m_name; }
	const QString& iconName() const { return m_iconName; }
	void setIconImage(const QImage& iconImage) { m_iconImage = iconImage; }
	const QImage& iconImage() const { return m_iconImage; }
	const QStringList& categories() const { return m_categories; }

private:
	QString m_path;
	QDateTime m_lastUpdated;

	bool m_isNoDisplay;
	QString m_name;
	QString m_exec;
	QString m_iconName;
	QImage m_iconImage;
	QStringList m_categories;
};

Q_DECLARE_METATYPE(DesktopApplication)

class QDir;
class QTimer;
class QFileSystemWatcher;

class DesktopApplications: public QThread
{
	Q_OBJECT
public:
	DesktopApplications();
	~DesktopApplications();

	static DesktopApplications* instance() { return m_instance; }

	QList<DesktopApplication> applications();
	DesktopApplication applicationFromPath(const QString& path);
	void launch(const QString& path);

signals:
	void applicationUpdated(const DesktopApplication& app);
	void applicationRemoved(const QString& path);

protected:
	void run();

private slots:
	void directoryChanged(const QString& path);
	void fileChanged(const QString& path);
	void refresh();

private:
	void traverse(const QDir& dir);

	static DesktopApplications* m_instance;

	QTimer* m_updateTimer;
	QFileSystemWatcher* m_watcher;
	QMutex m_applicationsMutex;
	QMap<QString, DesktopApplication> m_applications;
	QMutex m_tasksMutex;
	QWaitCondition m_tasksWaitCondition;
	bool m_abortWorker;
	QStringList m_fileTasks;
	QStringList m_imageTasks;
};

#endif
