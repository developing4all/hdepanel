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

#include "unifiediconservice.h"
#include "iconloader.h"
#include "desktopapplications.h"
#include "desktopdatastore.h"
#include "dpisupport.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>
#include <QStandardPaths>
#include <QFile>

UnifiedIconService* UnifiedIconService::m_instance = nullptr;

UnifiedIconService::UnifiedIconService(QObject* parent)
    : QObject(parent)
    , m_themeName(QIcon::themeName())
{
    if (m_themeName.isEmpty()) {
        m_themeName = "hicolor";
    }
}

UnifiedIconService::~UnifiedIconService()
{
    m_instance = nullptr;
}

UnifiedIconService* UnifiedIconService::instance()
{
    if (!m_instance) {
        m_instance = new UnifiedIconService();
    }
    return m_instance;
}

QIcon UnifiedIconService::loadIcon(const QString& iconName, int size, const QString& fallbackIcon)
{
    QMutexLocker lock(&m_mutex);
    
    if (iconName.isEmpty()) {
        return QIcon::fromTheme(fallbackIcon);
    }
    // Cache lookup
    const QString cacheKey = makeCacheKey(iconName, size);
    if (m_imageCache.contains(cacheKey)) {
        return QIcon(QPixmap::fromImage(m_imageCache.value(cacheKey)));
    }
    if (m_negativeCache.contains(cacheKey)) {
        return QIcon::fromTheme(fallbackIcon);
    }

    // Disk cache lookup (skip if negative marker exists)
    if (!isNegativeOnDisk(iconName, size)) {
        QImage diskImg = loadFromDiskCache(iconName, size);
        if (!diskImg.isNull()) {
            m_imageCache.insert(cacheKey, diskImg);
            return QIcon(QPixmap::fromImage(diskImg));
        }
    } else {
        return QIcon::fromTheme(fallbackIcon);
    }

    QImage image = loadIconInternal(iconName, size, fallbackIcon);
    if (!image.isNull()) {
        m_imageCache.insert(cacheKey, image);
        writeToDiskCache(iconName, size, image);
    } else {
        m_negativeCache.insert(cacheKey);
        markNegativeOnDisk(iconName, size);
    }
    return QIcon(QPixmap::fromImage(image));
}

QImage UnifiedIconService::loadIconAsImage(const QString& iconName, int size, const QString& fallbackIcon)
{
    QMutexLocker lock(&m_mutex);
    const QString cacheKey = makeCacheKey(iconName, size);
    if (m_imageCache.contains(cacheKey)) {
        return m_imageCache.value(cacheKey);
    }
    if (m_negativeCache.contains(cacheKey)) {
        QIcon fallback = QIcon::fromTheme(fallbackIcon);
        return fallback.pixmap(size, size).toImage();
    }
    // Disk cache lookup
    if (!isNegativeOnDisk(iconName, size)) {
        QImage diskImg = loadFromDiskCache(iconName, size);
        if (!diskImg.isNull()) {
            m_imageCache.insert(cacheKey, diskImg);
            return diskImg;
        }
    } else {
        QIcon fallback = QIcon::fromTheme(fallbackIcon);
        return fallback.pixmap(size, size).toImage();
    }
    QImage img = loadIconInternal(iconName, size, fallbackIcon);
    if (!img.isNull()) {
        m_imageCache.insert(cacheKey, img);
        writeToDiskCache(iconName, size, img);
    } else {
        m_negativeCache.insert(cacheKey);
        markNegativeOnDisk(iconName, size);
    }
    return img;
}

QIcon UnifiedIconService::loadApplicationIcon(const DesktopApplication& app, int size)
{
    QMutexLocker lock(&m_mutex);
    
    // First try the icon image if it's already loaded
    if (!app.iconImage().isNull()) {
        return QIcon(QPixmap::fromImage(app.iconImage()));
    }
    
    // Try the icon name from the desktop file
    QString iconName = app.iconName();
    if (!iconName.isEmpty()) {
        const QString cacheKey = makeCacheKey(iconName, size);
        if (m_imageCache.contains(cacheKey)) {
            return QIcon(QPixmap::fromImage(m_imageCache.value(cacheKey)));
        }
        if (!m_negativeCache.contains(cacheKey)) {
            QImage image = loadIconInternal(iconName, size, "application-x-executable");
            if (!image.isNull()) {
                m_imageCache.insert(cacheKey, image);
                return QIcon(QPixmap::fromImage(image));
            } else {
                m_negativeCache.insert(cacheKey);
            }
        }
    }
    
    // Try application name variations
    QStringList fallbackNames = generateFallbackNames(app.name(), iconName);
    for (const QString& name : fallbackNames) {
        const QString cacheKey = makeCacheKey(name, size);
        if (m_imageCache.contains(cacheKey)) {
            return QIcon(QPixmap::fromImage(m_imageCache.value(cacheKey)));
        }
        if (m_negativeCache.contains(cacheKey)) {
            continue;
        }
        QImage image = loadIconInternal(name, size, "application-x-executable");
        if (!image.isNull()) {
            m_imageCache.insert(cacheKey, image);
            return QIcon(QPixmap::fromImage(image));
        }
        m_negativeCache.insert(cacheKey);
    }
    
    // Final fallback
    return QIcon::fromTheme("application-x-executable");
}

QImage UnifiedIconService::loadApplicationIconAsImage(const DesktopApplication& app, int size)
{
    QMutexLocker lock(&m_mutex);
    
    // First try the icon image if it's already loaded
    if (!app.iconImage().isNull()) {
        return app.iconImage();
    }
    
    // Try the icon name from the desktop file
    QString iconName = app.iconName();
    if (!iconName.isEmpty()) {
        const QString cacheKey = makeCacheKey(iconName, size);
        if (m_imageCache.contains(cacheKey)) {
            return m_imageCache.value(cacheKey);
        }
        if (!m_negativeCache.contains(cacheKey)) {
            QImage image = loadIconInternal(iconName, size, "application-x-executable");
            if (!image.isNull()) {
                m_imageCache.insert(cacheKey, image);
                return image;
            }
            m_negativeCache.insert(cacheKey);
        }
    }
    
    // Try application name variations
    QStringList fallbackNames = generateFallbackNames(app.name(), iconName);
    for (const QString& name : fallbackNames) {
        const QString cacheKey = makeCacheKey(name, size);
        if (m_imageCache.contains(cacheKey)) {
            return m_imageCache.value(cacheKey);
        }
        if (m_negativeCache.contains(cacheKey)) {
            continue;
        }
        QImage image = loadIconInternal(name, size, "application-x-executable");
        if (!image.isNull()) {
            m_imageCache.insert(cacheKey, image);
            return image;
        }
        m_negativeCache.insert(cacheKey);
    }
    
    // Try generic application type fallbacks
    QStringList genericFallbacks;
    if (app.name().contains("Terminal", Qt::CaseInsensitive)) {
        genericFallbacks << "utilities-terminal" << "terminal" << "console";
    } else if (app.name().contains("Editor", Qt::CaseInsensitive) || 
               app.name().contains("Text", Qt::CaseInsensitive)) {
        genericFallbacks << "text-editor" << "accessories-text-editor" << "document-edit";
    } else if (app.name().contains("Browser", Qt::CaseInsensitive) || 
               app.name().contains("Web", Qt::CaseInsensitive)) {
        genericFallbacks << "web-browser" << "internet-web-browser" << "applications-internet";
    } else {
        genericFallbacks << "applications-office" << "applications-development" << "applications-system";
    }
    
    for (const QString& fallback : genericFallbacks) {
        QImage image = loadIconInternal(fallback, size, "application-x-executable");
        if (!image.isNull()) {
            return image;
        }
    }
    
    // Final fallback
    QIcon fallbackIcon = QIcon::fromTheme("application-x-executable");
    return fallbackIcon.pixmap(size, size).toImage();
}

QIcon UnifiedIconService::loadApplicationIcon(const QString& appId, const QString& wmClass, int size)
{
    QMutexLocker lock(&m_mutex);
    
    // Try to find matching desktop application
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    if (dataStore) {
        // Search for applications by executable name or WM class
        QList<DesktopEntryData> matches = dataStore->searchByExecutable(appId);
        if (matches.isEmpty() && !wmClass.isEmpty()) {
            // Try searching by WM class in the startupWMClass field
            QList<DesktopEntryData> allEntries = dataStore->getAllDesktopEntries();
            foreach (const DesktopEntryData& entry, allEntries) {
                if (entry.startupWMClass == wmClass) {
                    matches.append(entry);
                    break;
                }
            }
        }
        if (!matches.isEmpty()) {
            DesktopApplication app = dataStore->convertToDesktopApplication(matches.first());
            return loadApplicationIcon(app, size);
        }
    }
    
    // Try direct icon loading with appId and wmClass
    QStringList namesToTry;
    if (!appId.isEmpty()) {
        namesToTry.append(appId);
    }
    if (!wmClass.isEmpty() && wmClass != appId) {
        namesToTry.append(wmClass);
    }
    
    for (const QString& name : namesToTry) {
        QImage image = loadIconInternal(name, size, "application-x-executable");
        if (!image.isNull()) {
            return QIcon(QPixmap::fromImage(image));
        }
    }
    
    // Final fallback
    return QIcon::fromTheme("application-x-executable");
}

QImage UnifiedIconService::loadApplicationIconAsImage(const QString& appId, const QString& wmClass, int size)
{
    QMutexLocker lock(&m_mutex);
    
    // Try to find matching desktop application
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    if (dataStore) {
        // Search for applications by executable name or WM class
        QList<DesktopEntryData> matches = dataStore->searchByExecutable(appId);
        if (matches.isEmpty() && !wmClass.isEmpty()) {
            // Try searching by WM class in the startupWMClass field
            QList<DesktopEntryData> allEntries = dataStore->getAllDesktopEntries();
            foreach (const DesktopEntryData& entry, allEntries) {
                if (entry.startupWMClass == wmClass) {
                    matches.append(entry);
                    break;
                }
            }
        }
        if (!matches.isEmpty()) {
            DesktopApplication app = dataStore->convertToDesktopApplication(matches.first());
            return loadApplicationIconAsImage(app, size);
        }
    }
    
    // Try direct icon loading with appId and wmClass
    QStringList namesToTry;
    if (!appId.isEmpty()) {
        namesToTry.append(appId);
    }
    if (!wmClass.isEmpty() && wmClass != appId) {
        namesToTry.append(wmClass);
    }
    
    for (const QString& name : namesToTry) {
        QImage image = loadIconInternal(name, size, "application-x-executable");
        if (!image.isNull()) {
            return image;
        }
    }
    
    // Final fallback
    QIcon fallbackIcon = QIcon::fromTheme("application-x-executable");
    return fallbackIcon.pixmap(size, size).toImage();
}

QString UnifiedIconService::currentThemeName() const
{
    QMutexLocker lock(&m_mutex);
    return m_themeName;
}

void UnifiedIconService::setThemeName(const QString& themeName)
{
    QMutexLocker lock(&m_mutex);
    
    QString newTheme = themeName;
    if (newTheme.isEmpty()) {
        newTheme = "hicolor";
    }
    
    if (m_themeName != newTheme) {
        // Clear disk cache for old theme before switching
        clearDiskCacheForTheme(m_themeName);
        m_themeName = newTheme;
        QIcon::setThemeName(m_themeName);
        
        // Clear IconLoader cache
        if (IconLoader::instance()) {
            IconLoader::instance()->clearCache();
        }
        // Clear local caches as theme changed
        m_imageCache.clear();
        m_negativeCache.clear();
        m_loggedDirectSearch.clear();
        
        emit themeChanged(m_themeName);
    }
}

void UnifiedIconService::clearCache()
{
    QMutexLocker lock(&m_mutex);
    
    if (IconLoader::instance()) {
        IconLoader::instance()->clearCache();
    }
    m_imageCache.clear();
    m_negativeCache.clear();
    m_loggedDirectSearch.clear();
    clearDiskCacheForTheme(m_themeName);
}

QStringList UnifiedIconService::availableThemes() const
{
    QStringList themes;
    
    // Get themes from standard locations
    QStringList searchPaths;
    searchPaths.append(QString(getenv("HOME")) + "/.icons");
    
    QString xdgDataDirs;
    char* xdgDataDirsEnv = getenv("XDG_DATA_DIRS");
    if (xdgDataDirsEnv != NULL) {
        xdgDataDirs = xdgDataDirsEnv;
    } else {
        xdgDataDirs = "/usr/local/share/:/usr/share/";
    }
    
    QStringList dirs = xdgDataDirs.split(':');
    foreach(const QString& dir, dirs) {
        searchPaths.append(dir + "/icons");
    }
    
    foreach(const QString& path, searchPaths) {
        QDir dir(path);
        if (dir.exists()) {
            QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            foreach(const QString& entry, entries) {
                if (QFileInfo(dir.absoluteFilePath(entry + "/index.theme")).exists()) {
                    if (!themes.contains(entry)) {
                        themes.append(entry);
                    }
                }
            }
        }
    }
    
    return themes;
}

QImage UnifiedIconService::loadIconInternal(const QString& iconName, int size, const QString& fallbackIcon)
{
    if (iconName.isEmpty()) {
        QIcon fallback = QIcon::fromTheme(fallbackIcon);
        return fallback.pixmap(size, size).toImage();
    }
    
    QElapsedTimer iconTimer;
    iconTimer.start();
    
    // Try custom IconLoader first (current theme only - fast)
    QImage result = tryCustomIconLoader(iconName, size);
    if (!result.isNull()) {
        return result;
    }
    
    // PERFORMANCE FIX: Try common icon paths directly without expensive theme operations
    result = tryCommonIconPaths(iconName, size);
    if (!result.isNull()) {
        return result;
    }
    
    // Final fallback - only use Qt for the fallback icon
    QIcon fallback = QIcon::fromTheme(fallbackIcon);
    return fallback.pixmap(size, size).toImage();
}

QImage UnifiedIconService::tryCommonIconPaths(const QString& iconName, int size)
{
    // PERFORMANCE FIX: Check only the most common icon paths without expensive operations
    // This avoids the slow QIcon::setThemeName() and extensive file system scans
    
    QStringList commonPaths;
    
    // Add XDG_DATA_DIRS paths
    QStringList xdgDataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    foreach (const QString& dataDir, xdgDataDirs) {
        // Try exact size first
        commonPaths << dataDir + "/icons/hicolor/" + QString::number(size) + "x" + QString::number(size) + "/apps/" + iconName + ".png";
        commonPaths << dataDir + "/icons/hicolor/" + QString::number(size) + "x" + QString::number(size) + "/apps/" + iconName + ".svg";
        commonPaths << dataDir + "/icons/hicolor/" + QString::number(size) + "x" + QString::number(size) + "/apps/" + iconName + ".xpm";
        
        // Try common icon sizes (16, 24, 32, 48, 64, 128, 256) - very common for applications
        QList<int> commonSizes = {16, 24, 32, 48, 64, 128, 256};
        foreach (int commonSize, commonSizes) {
            if (commonSize != size) { // Don't duplicate the exact size
                commonPaths << dataDir + "/icons/hicolor/" + QString::number(commonSize) + "x" + QString::number(commonSize) + "/apps/" + iconName + ".png";
                commonPaths << dataDir + "/icons/hicolor/" + QString::number(commonSize) + "x" + QString::number(commonSize) + "/apps/" + iconName + ".svg";
                commonPaths << dataDir + "/icons/hicolor/" + QString::number(commonSize) + "x" + QString::number(commonSize) + "/apps/" + iconName + ".xpm";
            }
        }
        
        // Scalable icons (SVG) - very common for modern applications
        commonPaths << dataDir + "/icons/hicolor/scalable/apps/" + iconName + ".svg";
    }
    
    // Add /usr/share/pixmaps (very common location)
    commonPaths << "/usr/share/pixmaps/" + iconName + ".png";
    commonPaths << "/usr/share/pixmaps/" + iconName + ".svg";
    commonPaths << "/usr/share/pixmaps/" + iconName + ".xpm";
    
    // Add ~/.local/share/icons/ (user-installed icons)
    QString homeDir = QDir::homePath();
    commonPaths << homeDir + "/.local/share/icons/" + iconName + ".png";
    commonPaths << homeDir + "/.local/share/icons/" + iconName + ".svg";
    commonPaths << homeDir + "/.local/share/icons/" + iconName + ".xpm";
    
    // Add current theme paths (if we have a theme name)
    if (!m_themeName.isEmpty()) {
        foreach (const QString& dataDir, xdgDataDirs) {
            commonPaths << dataDir + "/icons/" + m_themeName + "/" + QString::number(size) + "x" + QString::number(size) + "/apps/" + iconName + ".png";
            commonPaths << dataDir + "/icons/" + m_themeName + "/" + QString::number(size) + "x" + QString::number(size) + "/apps/" + iconName + ".svg";
        }
    }
    
    // Try each path
    foreach (const QString& path, commonPaths) {
        if (QFile::exists(path)) {
            QImage image(path);
            if (!image.isNull()) {
                // Scale to exact size if needed
                if (image.width() != size || image.height() != size) {
                    image = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                return image;
            }
        }
    }
    
    return QImage();
}

QImage UnifiedIconService::tryCustomIconLoader(const QString& iconName, int size)
{
    if (!IconLoader::instance()) {
        return QImage();
    }
    
    // PERFORMANCE FIX: Only try current theme to avoid expensive fallback theme operations
    // IconLoader::loadIcon() with different themes is also expensive (100+ ms per call)
    QImage result = IconLoader::instance()->loadIcon(m_themeName, iconName, adjustHardcodedPixelSize(size));
    if (!result.isNull()) {
        return result;
    }
    
    // Skip fallback themes - they're too expensive
    return QImage();
}

QImage UnifiedIconService::tryQtIconTheme(const QString& iconName, int size)
{
    // Try current theme only - no fallback themes to avoid expensive QIcon::setThemeName() calls
    QIcon icon = QIcon::fromTheme(iconName);
    if (!icon.isNull()) {
        QPixmap pixmap = icon.pixmap(adjustHardcodedPixelSize(size));
        if (!pixmap.isNull()) {
            return pixmap.toImage();
        }
    }
    
    // PERFORMANCE FIX: Don't try any fallback themes with QIcon::setThemeName()
    // This is extremely expensive (1000+ ms per call) because it reloads the entire theme
    // Let the CustomIconLoader handle fallback themes instead
    return QImage();
}

QImage UnifiedIconService::tryDifferentSizes(const QString& iconName, int preferredSize)
{
    if (!IconLoader::instance()) {
        return QImage();
    }
    
    QList<int> sizes = {16, 24, 32, 48, 64, 128, 256};
    
    // Try sizes close to preferred size first
    QList<int> orderedSizes;
    orderedSizes.append(preferredSize);
    
    for (int size : sizes) {
        if (size != preferredSize) {
            orderedSizes.append(size);
        }
    }
    
    // PERFORMANCE FIX: Only try current theme to avoid expensive theme switching
    // The IconLoader::loadIcon() with different themes is also expensive
    for (int size : orderedSizes) {
        QImage result = IconLoader::instance()->loadIcon(m_themeName, iconName, adjustHardcodedPixelSize(size));
        if (!result.isNull()) {
            // Scale to preferred size if needed
            if (result.width() != adjustHardcodedPixelSize(preferredSize) || 
                result.height() != adjustHardcodedPixelSize(preferredSize)) {
                result = result.scaled(adjustHardcodedPixelSize(preferredSize), adjustHardcodedPixelSize(preferredSize), 
                                     Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            return result;
        }
    }
    
    return QImage();
}

QImage UnifiedIconService::tryDirectFileSearch(const QString& iconName, int size)
{
    // Reduce duplicate log spam: only log first time per icon name per session
    if (!m_loggedDirectSearch.contains(iconName)) {
        m_loggedDirectSearch.insert(iconName);
    }
    
    // Fast path: if iconName is an absolute or relative file path, try load directly
    QFileInfo fi(iconName);
    if (fi.isAbsolute() || iconName.contains('/')) {
        QImage direct;
        if (direct.load(iconName)) {
            if (direct.width() != adjustHardcodedPixelSize(size) || direct.height() != adjustHardcodedPixelSize(size)) {
                direct = direct.scaled(adjustHardcodedPixelSize(size), adjustHardcodedPixelSize(size), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            return direct;
        }
        // If name has no extension, try common ones next to the provided path base
        if (!fi.suffix().length()) {
            QString basePath = iconName;
            QStringList extensions = {".png", ".svg", ".xpm", ".ico"};
            for (const QString& ext : extensions) {
                QImage img;
                if (img.load(basePath + ext)) {
                    if (img.width() != adjustHardcodedPixelSize(size) || img.height() != adjustHardcodedPixelSize(size)) {
                        img = img.scaled(adjustHardcodedPixelSize(size), adjustHardcodedPixelSize(size), Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    return img;
                }
            }
        }
    }
    
    // Get XDG data directories
    QStringList searchPaths;
    
    // Add home directory
    searchPaths.append(QString(getenv("HOME")) + "/.local/share");
    
    // Add XDG data directories
    QString xdgDataDirs;
    char* xdgDataDirsEnv = getenv("XDG_DATA_DIRS");
    if (xdgDataDirsEnv != NULL) {
        xdgDataDirs = xdgDataDirsEnv;
    } else {
        xdgDataDirs = "/usr/local/share/:/usr/share/";
    }
    
    QStringList dirs = xdgDataDirs.split(':');
    foreach(const QString& dir, dirs) {
        if (!dir.isEmpty()) {
            searchPaths.append(dir);
        }
    }
    
    // Common icon file extensions
    QStringList extensions = {".png", ".svg", ".xpm", ".ico"};
    
    // Try different variations of the icon name
    QStringList iconVariations;
    iconVariations.append(iconName);
    
    // Add extensions if not already present
    if (!iconName.contains('.')) {
        foreach(const QString& ext, extensions) {
            iconVariations.append(iconName + ext);
        }
    }
    
    // Search in each directory
    foreach(const QString& basePath, searchPaths) {
        // Search in pixmaps directory
        QString pixmapsPath = basePath + "/pixmaps";
        foreach(const QString& iconFile, iconVariations) {
            QString fullPath = pixmapsPath + "/" + iconFile;
            QImage image;
            if (image.load(fullPath)) {
                // Scale to desired size if needed
                if (image.width() != adjustHardcodedPixelSize(size) || 
                    image.height() != adjustHardcodedPixelSize(size)) {
                    image = image.scaled(adjustHardcodedPixelSize(size), adjustHardcodedPixelSize(size), 
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                return image;
            }
        }
        
        // Search in icons directory (for non-theme icons)
        QString iconsPath = basePath + "/icons";
        foreach(const QString& iconFile, iconVariations) {
            QString fullPath = iconsPath + "/" + iconFile;
            QImage image;
            if (image.load(fullPath)) {
                // Scale to desired size if needed
                if (image.width() != adjustHardcodedPixelSize(size) || 
                    image.height() != adjustHardcodedPixelSize(size)) {
                    image = image.scaled(adjustHardcodedPixelSize(size), adjustHardcodedPixelSize(size), 
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                return image;
            }
        }
        
        // Search in theme subdirectories within icons
        QDir iconsDir(iconsPath);
        if (iconsDir.exists()) {
            QStringList themeDirs = iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            foreach(const QString& themeDir, themeDirs) {
                QString themePath = iconsPath + "/" + themeDir;
                
                // Search in apps subdirectory
                QString appsThemePath = themePath + "/apps";
                foreach(const QString& iconFile, iconVariations) {
                    QString fullPath = appsThemePath + "/" + iconFile;
                    QImage image;
                    if (image.load(fullPath)) {
                        // Scale to desired size if needed
                        if (image.width() != adjustHardcodedPixelSize(size) || 
                            image.height() != adjustHardcodedPixelSize(size)) {
                            image = image.scaled(adjustHardcodedPixelSize(size), adjustHardcodedPixelSize(size), 
                                               Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        }
                        return image;
                    }
                }
                
                // Search in different size directories
                QStringList sizeDirs = {"16x16", "22x22", "24x24", "32x32", "48x48", "64x64", "128x128", "256x256"};
                foreach(const QString& sizeDir, sizeDirs) {
                    QString sizePath = themePath + "/" + sizeDir + "/apps";
                    foreach(const QString& iconFile, iconVariations) {
                        QString fullPath = sizePath + "/" + iconFile;
                        QImage image;
                        if (image.load(fullPath)) {
                            // Scale to desired size if needed
                            if (image.width() != adjustHardcodedPixelSize(size) || 
                                image.height() != adjustHardcodedPixelSize(size)) {
                                image = image.scaled(adjustHardcodedPixelSize(size), adjustHardcodedPixelSize(size), 
                                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
                            }
                            return image;
                        }
                    }
                }
            }
        }
        
        // Search in applications directory (for app-specific icons)
        QString appsPath = basePath + "/applications";
        foreach(const QString& iconFile, iconVariations) {
            QString fullPath = appsPath + "/" + iconFile;
            QImage image;
            if (image.load(fullPath)) {
                // Scale to desired size if needed
                if (image.width() != adjustHardcodedPixelSize(size) || 
                    image.height() != adjustHardcodedPixelSize(size)) {
                    image = image.scaled(adjustHardcodedPixelSize(size), adjustHardcodedPixelSize(size), 
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
                return image;
            }
        }
    }
    
    return QImage();
}

// --- Persistent disk cache implementation ---

QString UnifiedIconService::cacheBaseDirForTheme(const QString& theme) const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty()) {
        base = QString(getenv("HOME")) + "/.cache";
    }
    QDir dir(base + "/hdepanel/icons/" + theme);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return dir.absolutePath();
}

QString UnifiedIconService::cacheFilePath(const QString& name, int size) const
{
    QString safe = name;
    safe.replace('/', '_');
    return cacheBaseDirForTheme(m_themeName) + "/" + safe + "_" + QString::number(size) + ".png";
}

QString UnifiedIconService::negativeFilePath(const QString& name, int size) const
{
    QString safe = name;
    safe.replace('/', '_');
    return cacheBaseDirForTheme(m_themeName) + "/" + safe + "_" + QString::number(size) + ".neg";
}

QImage UnifiedIconService::loadFromDiskCache(const QString& name, int size) const
{
    QString path = cacheFilePath(name, size);
    QImage img;
    if (QFile::exists(path)) {
        img.load(path);
        return img;
    }
    return QImage();
}

void UnifiedIconService::writeToDiskCache(const QString& name, int size, const QImage& image) const
{
    if (image.isNull()) return;
    QString path = cacheFilePath(name, size);
    QDir dir = QFileInfo(path).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    image.save(path, "PNG");
}

bool UnifiedIconService::isNegativeOnDisk(const QString& name, int size) const
{
    return QFile::exists(negativeFilePath(name, size));
}

void UnifiedIconService::markNegativeOnDisk(const QString& name, int size) const
{
    QString npath = negativeFilePath(name, size);
    QDir dir = QFileInfo(npath).dir();
    if (!dir.exists()) dir.mkpath(".");
    QFile f(npath);
    if (!f.exists()) {
        if (f.open(QIODevice::WriteOnly)) {
            f.write("0");
            f.close();
        }
    }
}

void UnifiedIconService::clearDiskCacheForTheme(const QString& theme) const
{
    QDir dir(cacheBaseDirForTheme(theme));
    if (dir.exists()) {
        dir.setFilter(QDir::Files);
        QFileInfoList files = dir.entryInfoList();
        for (const QFileInfo& fi : files) {
            QFile::remove(fi.absoluteFilePath());
        }
    }
}

QStringList UnifiedIconService::generateFallbackNames(const QString& appName, const QString& iconName) const
{
    QStringList names;
    
    // Add original icon name if provided
    if (!iconName.isEmpty()) {
        names.append(iconName);
    }
    
    // Generate variations from app name
    QString baseName = appName.toLower();
    
    // Direct name
    names.append(baseName);
    
    // Replace spaces with dashes
    names.append(baseName.replace(' ', '-'));
    
    // Replace spaces with underscores
    names.append(baseName.replace(' ', '_'));
    
    // Remove common prefixes
    QString cleanName = baseName;
    if (cleanName.startsWith("org.") || cleanName.startsWith("com.") || 
        cleanName.startsWith("net.") || cleanName.startsWith("io.")) {
        QStringList parts = cleanName.split('.');
        if (parts.size() > 1) {
            cleanName = parts.last();
            names.append(cleanName);
        }
    }
    
    // Remove common suffixes
    if (cleanName.endsWith("-bin") || cleanName.endsWith("-gtk") || 
        cleanName.endsWith("-qt") || cleanName.endsWith("-desktop")) {
        cleanName = cleanName.left(cleanName.lastIndexOf('-'));
        names.append(cleanName);
    }
    
    // Special handling for known applications
    if (appName.contains("Firefox", Qt::CaseInsensitive)) {
        names.append("firefox");
        names.append("mozilla-firefox");
        names.append("web-browser");
    }
    if (appName.contains("Cursor", Qt::CaseInsensitive)) {
        names.append("co.anysphere.cursor");
        names.append("cursor");
        names.append("text-editor");
        names.append("code");
        names.append("accessories-text-editor");
    }
    if (appName.contains("Visual Studio Code", Qt::CaseInsensitive) || 
        appName.contains("Code", Qt::CaseInsensitive)) {
        names.append("vscode");
        names.append("visual-studio-code");
        names.append("code");
        names.append("text-editor");
        names.append("accessories-text-editor");
    }
    if (appName.contains("Files", Qt::CaseInsensitive)) {
        names.append("org.gnome.Nautilus");
        names.append("nautilus");
        names.append("file-manager");
        names.append("system-file-manager");
        names.append("folder");
    }
    if (appName.contains("Terminal", Qt::CaseInsensitive)) {
        names.append("terminal");
        names.append("utilities-terminal");
        names.append("gnome-terminal");
        names.append("xterm");
        names.append("console");
    }
    if (appName.contains("QTerminal", Qt::CaseInsensitive)) {
        names.append("qterminal");
        names.append("utilities-terminal");
        names.append("terminal");
    }
    
    // Generic fallbacks based on categories
    if (appName.contains("Editor", Qt::CaseInsensitive) || 
        appName.contains("Text", Qt::CaseInsensitive)) {
        names.append("text-editor");
        names.append("accessories-text-editor");
    }
    if (appName.contains("Browser", Qt::CaseInsensitive) || 
        appName.contains("Web", Qt::CaseInsensitive)) {
        names.append("web-browser");
        names.append("internet-web-browser");
    }
    if (appName.contains("Terminal", Qt::CaseInsensitive) || 
        appName.contains("Console", Qt::CaseInsensitive)) {
        names.append("utilities-terminal");
        names.append("terminal");
    }
    
    // Remove duplicates
    names.removeDuplicates();
    
    return names;
}

QStringList UnifiedIconService::getFallbackThemes() const
{
    QStringList availableThemesList = availableThemes();
    QStringList commonFallbacks = {"hicolor", "gnome", "default"};
    
    foreach(const QString& fallback, commonFallbacks) {
        if (!availableThemesList.contains(fallback)) {
            availableThemesList.append(fallback);
        }
    }
    
    return availableThemesList;
}
