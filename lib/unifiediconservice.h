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

#ifndef UNIFIEDICONSERVICE_H
#define UNIFIEDICONSERVICE_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtCore/QMutex>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QStringBuilder>

class DesktopApplication;

/**
 * @brief Unified icon loading service that provides consistent icon loading across all components
 * 
 * This service centralizes all icon loading logic and provides multiple fallback strategies
 * to ensure icons are found and displayed correctly throughout the application.
 */
class UnifiedIconService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance of the service
     * @return Pointer to the singleton instance
     */
    static UnifiedIconService* instance();

    /**
     * @brief Load an icon by name with comprehensive fallback strategies
     * @param iconName The name of the icon to load
     * @param size The desired size of the icon
     * @param fallbackIcon Fallback icon name if the primary fails (default: "application-x-executable")
     * @return QIcon object, never null (always returns at least the fallback icon)
     */
    QIcon loadIcon(const QString& iconName, int size = 32, const QString& fallbackIcon = "application-x-executable");

    /**
     * @brief Load an icon as QImage with comprehensive fallback strategies
     * @param iconName The name of the icon to load
     * @param size The desired size of the icon
     * @param fallbackIcon Fallback icon name if the primary fails (default: "application-x-executable")
     * @return QImage object, never null (always returns at least the fallback icon)
     */
    QImage loadIconAsImage(const QString& iconName, int size = 32, const QString& fallbackIcon = "application-x-executable");

    /**
     * @brief Load an icon for a desktop application with smart matching
     * @param app The desktop application object
     * @param size The desired size of the icon
     * @return QIcon object, never null
     */
    QIcon loadApplicationIcon(const DesktopApplication& app, int size = 32);

    /**
     * @brief Load an icon for a desktop application as QImage with smart matching
     * @param app The desktop application object
     * @param size The desired size of the icon
     * @return QImage object, never null
     */
    QImage loadApplicationIconAsImage(const DesktopApplication& app, int size = 32);

    /**
     * @brief Load an icon for an application by app ID and window class
     * @param appId The application ID
     * @param wmClass The window manager class
     * @param size The desired size of the icon
     * @return QIcon object, never null
     */
    QIcon loadApplicationIcon(const QString& appId, const QString& wmClass = QString(), int size = 32);

    /**
     * @brief Load an icon for an application by app ID and window class as QImage
     * @param appId The application ID
     * @param wmClass The window manager class
     * @param size The desired size of the icon
     * @return QImage object, never null
     */
    QImage loadApplicationIconAsImage(const QString& appId, const QString& wmClass = QString(), int size = 32);

    /**
     * @brief Get the current icon theme name
     * @return Current icon theme name
     */
    QString currentThemeName() const;

    /**
     * @brief Set the icon theme name
     * @param themeName The new theme name
     */
    void setThemeName(const QString& themeName);

    /**
     * @brief Clear the icon cache
     */
    void clearCache();

    /**
     * @brief Get a list of available icon themes
     * @return List of available theme names
     */
    QStringList availableThemes() const;

signals:
    /**
     * @brief Emitted when the icon theme changes
     * @param themeName The new theme name
     */
    void themeChanged(const QString& themeName);

private:
    explicit UnifiedIconService(QObject* parent = nullptr);
    ~UnifiedIconService();

    // Disable copy constructor and assignment operator
    UnifiedIconService(const UnifiedIconService&) = delete;
    UnifiedIconService& operator=(const UnifiedIconService&) = delete;

    /**
     * @brief Internal method to load icon with multiple fallback strategies
     * @param iconName The name of the icon to load
     * @param size The desired size of the icon
     * @param fallbackIcon Fallback icon name
     * @return QImage object, never null
     */
    QImage loadIconInternal(const QString& iconName, int size, const QString& fallbackIcon);

    /**
     * @brief Try to load icon using custom IconLoader
     * @param iconName The name of the icon to load
     * @param size The desired size of the icon
     * @return QImage object, null if failed
     */
    QImage tryCustomIconLoader(const QString& iconName, int size);

    /**
     * @brief Try to load icon using Qt's icon theme system
     * @param iconName The name of the icon to load
     * @param size The desired size of the icon
     * @return QImage object, null if failed
     */
    QImage tryQtIconTheme(const QString& iconName, int size);

    /**
     * @brief Try to load icon using different sizes
     * @param iconName The name of the icon to load
     * @param preferredSize The preferred size
     * @return QImage object, null if failed
     */
    QImage tryDifferentSizes(const QString& iconName, int preferredSize);

    /**
     * @brief Try to load icon by searching XDG data directories directly
     * @param iconName The name of the icon to load
     * @param size The desired size of the icon
     * @return QImage object, null if failed
     */
    QImage tryDirectFileSearch(const QString& iconName, int size);

    /**
     * @brief Generate fallback icon names for an application
     * @param appName The application name
     * @param iconName The original icon name
     * @return List of potential icon names to try
     */
    QStringList generateFallbackNames(const QString& appName, const QString& iconName = QString()) const;

    static UnifiedIconService* m_instance;
    QString m_themeName;
    mutable QMutex m_mutex;
    // Cache successful icon image lookups by key (theme|name|size)
    QHash<QString, QImage> m_imageCache;
    // Cache negative lookups to avoid repeated expensive searches (theme|name|size)
    QSet<QString> m_negativeCache;
    // Track which icon names have already logged tryDirectFileSearch to reduce log spam
    QSet<QString> m_loggedDirectSearch;
    inline QString makeCacheKey(const QString& name, int size) const {
        return m_themeName + "|" + name + "|" + QString::number(size);
    }

    // Persistent disk cache helpers (theme-scoped)
    QString cacheBaseDirForTheme(const QString& theme) const;
    QString cacheFilePath(const QString& name, int size) const;          // image cache file (.png)
    QString negativeFilePath(const QString& name, int size) const;       // negative marker (.neg)
    QImage loadFromDiskCache(const QString& name, int size) const;
    void writeToDiskCache(const QString& name, int size, const QImage& image) const;
    bool isNegativeOnDisk(const QString& name, int size) const;
    void markNegativeOnDisk(const QString& name, int size) const;
    void clearDiskCacheForTheme(const QString& theme) const;
    
    // Helper method to get fallback themes list
    QStringList getFallbackThemes() const;
    
    // Fast icon loading from common paths without expensive theme operations
    QImage tryCommonIconPaths(const QString& iconName, int size);
};

#endif // UNIFIEDICONSERVICE_H
