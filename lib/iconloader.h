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

#ifndef ICONLOADER_H
#define ICONLOADER_H

#include <QtCore/QMutex>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtGui/QImage>

// Unfortunately, Qt icon theme support is not flexible enough.
// I need to load icon images in another thread, and this is not possible with Qt implementation.
// Also, I would like to specify what theme to use when calling load function, instead of specifying it globally.
// Reinventing the wheel here...

class IconTheme
{
public:
	void init(const QString& themeName);
	QImage loadIcon(const QString& iconName, int size);

	const QStringList& inheritedThemes()
	{
		return m_inheritedThemes;
	}

private:
	struct IconDirectory
	{
		QString m_path;
		int m_size;
		bool m_scalable;
	};

	bool loadIconFromDirectory(QImage& result, const IconDirectory& iconDir, const QString& fileName);

	QString m_themeName;
	QStringList m_inheritedThemes;
	QList<IconDirectory> m_iconDirs;
};

class IconLoader
{
public:
	IconLoader();
	~IconLoader();

	static IconLoader* instance()
	{
		return m_instance;
	}

	const QStringList& iconSearchPaths()
	{
		return m_iconSearchPaths;
	}

	QImage loadIcon(const QString& themeName, const QString& iconName, int size);

private:
	QImage loadIconFromTheme(const QString& themeName, const QString& iconName, int size);

	static IconLoader* m_instance;
	QStringList m_iconSearchPaths;
	QMutex m_iconThemesMutex;
	QMap<QString, IconTheme> m_iconThemes;
};

#endif
