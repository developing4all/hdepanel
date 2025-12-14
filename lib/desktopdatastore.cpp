#include "desktopdatastore.h"
#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>
#include <QtCore/QCollator>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>
#include <QtCore/QLocale>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QCoreApplication>
#include <QtGui/QGuiApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QProcess>
#include "unifiediconservice.h"
#include "desktopapplications.h"

// DesktopEntryLoader implementation
DesktopEntryLoader::DesktopEntryLoader(QObject* parent)
    : QObject(parent)
{
}

DesktopEntryLoader::~DesktopEntryLoader()
{
}

void DesktopEntryLoader::loadAllDesktopEntries()
{
    QElapsedTimer loadTimer;
    loadTimer.start();
    
    QStringList searchPaths = getSearchPaths();
    
    int totalFiles = 0;
    foreach (const QString& path, searchPaths) {
        QElapsedTimer dirTimer;
        dirTimer.start();
        loadDesktopEntriesFromDirectory(path);
        totalFiles = countFilesInDirectory(path);
        qDebug() << "DesktopEntryLoader::loadAllDesktopEntries() - Directory" << path << "took" << dirTimer.elapsed() << "ms (" << totalFiles << "files)";
    }
    qDebug() << "DesktopEntryLoader::loadAllDesktopEntries() - Total loading took" << loadTimer.elapsed() << "ms";
    emit loadingCompleted();
}

int DesktopEntryLoader::countFilesInDirectory(const QString& directoryPath) const
{
    QDir dir(directoryPath);
    if (!dir.exists()) {
        return 0;
    }
    
    QStringList filters;
    filters << "*.desktop";
    
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::Readable);
    return fileList.size();
}

QStringList DesktopEntryLoader::getSearchPaths() const
{
    QStringList searchPaths;
    
    // Get XDG data directories
    QStringList xdgDataDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    
    // Add system-wide directories
    searchPaths << "/usr/share/applications";
    searchPaths << "/usr/local/share/applications";
    
    // Add XDG data directories
    foreach (const QString& dir, xdgDataDirs) {
        if (!searchPaths.contains(dir)) {
            searchPaths << dir;
        }
    }
    
    // Add user-specific directories
    QString homeDir = QDir::homePath();
    searchPaths << homeDir + "/.local/share/applications";
    
    return searchPaths;
}

void DesktopEntryLoader::loadDesktopEntriesFromDirectory(const QString& directoryPath)
{
    QDir dir(directoryPath);
    if (!dir.exists()) {
        return;
    }
    
    QStringList filters;
    filters << "*.desktop";
    
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::Readable);
    
    foreach (const QFileInfo& fileInfo, fileList) {
        QString filePath = fileInfo.absoluteFilePath();
        DesktopEntryData entryData = parseDesktopFile(filePath);
        
        if (entryData.isValid) {
            emit desktopEntryLoaded(entryData);
        }
    }
}

DesktopEntryData DesktopEntryLoader::parseDesktopFile(const QString& filePath)
{
    DesktopEntryData entry;
    entry.desktopFile = filePath;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        entry.errors << "Cannot open file";
        return entry;
    }
    
    QTextStream in(&file);
    QString currentSection;
    bool inDesktopEntry = false;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        
        // Check for section headers
        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2);
            inDesktopEntry = (currentSection == "Desktop Entry");
            continue;
        }
        
        // Only process Desktop Entry section
        if (!inDesktopEntry) {
            continue;
        }
        
        // Parse key=value pairs
        int equalPos = line.indexOf('=');
        if (equalPos == -1) {
            continue;
        }
        
        QString key = line.left(equalPos).trimmed();
        QString value = line.mid(equalPos + 1).trimmed();
        
        // Parse different keys
        if (key == "Type") {
            entry.type = value;
        } else if (key == "Version") {
            entry.version = value;
        } else if (key == "Name") {
            entry.name = value;
        } else if (key.startsWith("Name[") && key.endsWith("]")) {
            QString lang = key.mid(5, key.length() - 6);
            entry.localizedNames[lang] = value;
        } else if (key == "GenericName") {
            entry.genericName = value;
        } else if (key.startsWith("GenericName[") && key.endsWith("]")) {
            QString lang = key.mid(12, key.length() - 13);
            entry.localizedGenericNames[lang] = value;
        } else if (key == "Comment") {
            entry.comment = value;
        } else if (key.startsWith("Comment[") && key.endsWith("]")) {
            QString lang = key.mid(8, key.length() - 9);
            entry.localizedComments[lang] = value;
        } else if (key == "Icon") {
            entry.icon = value;
        } else if (key == "Exec") {
            entry.exec = value;
        } else if (key == "Path") {
            entry.path = value;
        } else if (key == "Terminal") {
            entry.terminal = (value.toLower() == "true");
        } else if (key == "Actions") {
            entry.actions = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "Categories") {
            // Parse categories more permissively - keep all categories even if they're invalid
            QStringList rawCategories = value.split(';');
            entry.categories.clear();
            for (const QString& category : rawCategories) {
                QString trimmed = category.trimmed();
                if (!trimmed.isEmpty()) {
                    entry.categories.append(trimmed);
                }
            }
        } else if (key == "Keywords") {
            entry.keywords = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "OnlyShowIn") {
            entry.onlyShowIn = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "NotShowIn") {
            entry.notShowIn = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "NoDisplay") {
            entry.noDisplay = (value.toLower() == "true");
        } else if (key == "Hidden") {
            entry.hidden = (value.toLower() == "true");
        } else if (key == "StartupNotify") {
            entry.startupNotify = (value.toLower() == "true");
        } else if (key == "StartupWMClass") {
            entry.startupWMClass = value;
        } else if (key == "MimeType") {
            entry.mimeTypes = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "URL") {
            entry.url = value;
        }
    }
    
    // Validate entry
    entry.isValid = !entry.type.isEmpty() && !entry.name.isEmpty();
    if (!entry.isValid) {
        entry.errors << "Missing required fields (Type or Name)";
    }
    
    return entry;
}

DesktopDataStore* DesktopDataStore::m_instance = nullptr;

DesktopDataStore* DesktopDataStore::instance()
{
    if (!m_instance) {
        m_instance = new DesktopDataStore();
    }
    return m_instance;
}

DesktopDataStore::DesktopDataStore(QObject* parent)
    : QObject(parent)
    , m_updateTimer(new QTimer(this))
    , m_workerThread(new QThread(this))
    , m_workerLoader(new DesktopEntryLoader())
    , m_loadingInProgress(false)
{
    // Register DesktopEntryData for Qt's signal/slot system
    qRegisterMetaType<DesktopEntryData>("DesktopEntryData");
    
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(1000); // 1 second delay for bulk updates
    
    connect(m_updateTimer, &QTimer::timeout, this, &DesktopDataStore::rebuildIndex);
    
    // Setup worker thread
    m_workerLoader->moveToThread(m_workerThread);
    
    // Connect worker signals to our slots
    connect(m_workerLoader, &DesktopEntryLoader::desktopEntryLoaded, 
            this, &DesktopDataStore::onDesktopEntryLoaded, Qt::QueuedConnection);
    connect(m_workerLoader, &DesktopEntryLoader::loadingCompleted, 
            this, &DesktopDataStore::onLoadingCompleted, Qt::QueuedConnection);
    connect(m_workerLoader, &DesktopEntryLoader::loadingError, 
            this, &DesktopDataStore::onLoadingError, Qt::QueuedConnection);
    
    // Connect thread finished to cleanup
    connect(m_workerThread, &QThread::finished, m_workerLoader, &QObject::deleteLater);
    
    // Start the worker thread
    m_workerThread->start();
}

DesktopDataStore::~DesktopDataStore()
{
    // Stop and cleanup worker thread
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(5000); // Wait up to 5 seconds
    }
    
    m_instance = nullptr;
}

QString DesktopEntryData::getLocalizedName(const QString& language) const
{
    if (language.isEmpty()) {
        return name;
    }
    
    // Try exact language match first
    if (localizedNames.contains(language)) {
        return localizedNames[language];
    }
    
    // Try language without country code (e.g., "en" for "en_US")
    QString langCode = language.split('_').first();
    for (auto it = localizedNames.begin(); it != localizedNames.end(); ++it) {
        if (it.key().startsWith(langCode + "_") || it.key() == langCode) {
            return it.value();
        }
    }
    
    // Fall back to default name
    return name;
}

QString DesktopEntryData::getLocalizedGenericName(const QString& language) const
{
    if (language.isEmpty()) {
        return genericName;
    }
    
    // Try exact language match first
    if (localizedGenericNames.contains(language)) {
        return localizedGenericNames[language];
    }
    
    // Try language without country code
    QString langCode = language.split('_').first();
    for (auto it = localizedGenericNames.begin(); it != localizedGenericNames.end(); ++it) {
        if (it.key().startsWith(langCode + "_") || it.key() == langCode) {
            return it.value();
        }
    }
    
    // Fall back to default generic name
    return genericName;
}

QString DesktopEntryData::getLocalizedComment(const QString& language) const
{
    if (language.isEmpty()) {
        return comment;
    }
    
    // Try exact language match first
    if (localizedComments.contains(language)) {
        return localizedComments[language];
    }
    
    // Try language without country code
    QString langCode = language.split('_').first();
    for (auto it = localizedComments.begin(); it != localizedComments.end(); ++it) {
        if (it.key().startsWith(langCode + "_") || it.key() == langCode) {
            return it.value();
        }
    }
    
    // Fall back to default comment
    return comment;
}

QStringList DesktopEntryData::getAllSearchableText() const
{
    QStringList searchableText;
    
    // Add all localized names
    searchableText.append(name);
    foreach (const QString& localizedName, localizedNames) {
        searchableText.append(localizedName);
    }
    
    // Add generic names
    if (!genericName.isEmpty()) {
        searchableText.append(genericName);
    }
    foreach (const QString& localizedGenericName, localizedGenericNames) {
        searchableText.append(localizedGenericName);
    }
    
    // Add keywords
    searchableText.append(keywords);
    
    // Add comments/descriptions
    if (!comment.isEmpty()) {
        searchableText.append(comment);
    }
    foreach (const QString& localizedComment, localizedComments) {
        searchableText.append(localizedComment);
    }
    
    // Add executable name (without path)
    if (!exec.isEmpty()) {
        QString execName = QFileInfo(exec).baseName();
        searchableText.append(execName);
    }
    
    // Add categories
    searchableText.append(categories);
    
    return searchableText;
}

bool DesktopEntryData::matchesQuery(const QString& query) const
{
    if (query.isEmpty()) {
        return true;
    }
    
    QString normalizedQuery = query.toLower().simplified();
    QStringList searchableText = getAllSearchableText();
    
    foreach (const QString& text, searchableText) {
        if (text.toLower().contains(normalizedQuery)) {
            return true;
        }
    }
    
    return false;
}

bool DesktopEntryData::shouldShow() const
{
    // Check if entry should be hidden
    if (noDisplay || hidden) {
        return false;
    }
    
    // Check desktop environment restrictions
    QString currentDesktop = QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP"));
    if (!currentDesktop.isEmpty()) {
        // Check notShowIn
        foreach (const QString& env, notShowIn) {
            if (currentDesktop.contains(env, Qt::CaseInsensitive)) {
                return false;
            }
        }
        
        // Check onlyShowIn (if specified)
        if (!onlyShowIn.isEmpty()) {
            bool found = false;
            foreach (const QString& env, onlyShowIn) {
                if (currentDesktop.contains(env, Qt::CaseInsensitive)) {
                    found = true;
                    break;
                }
            }
            
            // For Wayland compositors, be more permissive
            // If we're running on a Wayland compositor and the app is for a major DE,
            // show it anyway
            if (!found && currentDesktop.contains("wayland", Qt::CaseInsensitive)) {
                QStringList majorDEs = {"GNOME", "KDE", "XFCE", "LXDE", "LXQT", "MATE", "CINNAMON", "UNITY"};
                foreach (const QString& env, onlyShowIn) {
                    if (majorDEs.contains(env, Qt::CaseInsensitive)) {
                        found = true;
                        break;
                    }
                }
            }
            
            if (!found) {
                return false;
            }
        }
    }
    
    return true;
}

QString DesktopEntryData::getDisplayName(const QString& language) const
{
    // Prefer specific name over generic name
    QString name = getLocalizedName(language);
    if (!name.isEmpty()) {
        return name;
    }
    
    // Fall back to generic name if no specific name
    return getLocalizedGenericName(language);
}

DesktopEntryData DesktopDataStore::parseDesktopFile(const QString& filePath)
{
    DesktopEntryData entry;
    entry.desktopFile = filePath;
    entry.isValid = false;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        entry.errors.append("Cannot open file");
        return entry;
    }
    
    QTextStream in(&file);
    QString currentSection;
    QString currentLanguage;
    
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        
        // Check for section headers
        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2);
            continue;
        }
        
        // Only process [Desktop Entry] section
        if (currentSection != "Desktop Entry") {
            continue;
        }
        
        // Parse key-value pairs
        int equalPos = line.indexOf('=');
        if (equalPos == -1) {
            continue;
        }
        
        QString key = line.left(equalPos).trimmed();
        QString value = line.mid(equalPos + 1).trimmed();
        
        // Handle localized keys (e.g., Name[en_US])
        currentLanguage.clear();
        if (key.contains('[') && key.endsWith(']')) {
            int bracketPos = key.indexOf('[');
            currentLanguage = key.mid(bracketPos + 1, key.length() - bracketPos - 2);
            key = key.left(bracketPos);
        }
        
        // Parse standard desktop entry keys
        if (key == "Type") {
            entry.type = value;
        } else if (key == "Version") {
            entry.version = value;
        } else if (key == "Name") {
            if (value.contains("cursor")) {
                qDebug() << "DesktopEntry: Cursor name requested:" << value;
            }
        
            if (currentLanguage.isEmpty()) {
                entry.name = value;
            } else {
                entry.localizedNames[currentLanguage] = value;
            }
        } else if (key == "GenericName") {
            if (currentLanguage.isEmpty()) {
                entry.genericName = value;
            } else {
                entry.localizedGenericNames[currentLanguage] = value;
            }
        } else if (key == "Comment") {
            if (currentLanguage.isEmpty()) {
                entry.comment = value;
            } else {
                entry.localizedComments[currentLanguage] = value;
            }
        } else if (key == "Icon") {
            entry.icon = value;
            if (value.contains("cursor")) {
                qDebug() << "DesktopEntry: Cursor icon requested:" << value;
            }
        } else if (key == "Exec") {
            entry.exec = value;
        } else if (key == "Path") {
            entry.path = value;
        } else if (key == "Terminal") {
            entry.terminal = (value.toLower() == "true");
        } else if (key == "Actions") {
            entry.actions = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "Categories") {
            // Parse categories more permissively - keep all categories even if they're invalid
            QStringList rawCategories = value.split(';');
            entry.categories.clear();
            for (const QString& category : rawCategories) {
                QString trimmed = category.trimmed();
                if (!trimmed.isEmpty()) {
                    entry.categories.append(trimmed);
                }
            }
        } else if (key == "Keywords") {
            entry.keywords = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "OnlyShowIn") {
            entry.onlyShowIn = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "NotShowIn") {
            entry.notShowIn = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "NoDisplay") {
            entry.noDisplay = (value.toLower() == "true");
        } else if (key == "Hidden") {
            entry.hidden = (value.toLower() == "true");
        } else if (key == "StartupNotify") {
            entry.startupNotify = (value.toLower() == "true");
        } else if (key == "StartupWMClass") {
            entry.startupWMClass = value;
        } else if (key == "MimeType") {
            entry.mimeTypes = value.split(';', Qt::SkipEmptyParts);
        } else if (key == "URL") {
            entry.url = value;
        }
    }
    
    // Validate required fields
    if (entry.type.isEmpty()) {
        entry.errors.append("Missing Type field");
    } else if (entry.type == "Application") {
        if (entry.name.isEmpty()) {
            entry.errors.append("Missing Name field");
        }
        if (entry.exec.isEmpty()) {
            entry.errors.append("Missing Exec field");
        }
    }
    
    entry.isValid = entry.errors.isEmpty();
    return entry;
}

void DesktopDataStore::loadDesktopEntriesFromDirectory(const QString& directory)
{
    QDir dir(directory);
    if (!dir.exists()) {
        return;
    }
    
    QStringList filters;
    filters << "*.desktop";
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    
    foreach (const QFileInfo& fileInfo, files) {
        DesktopEntryData entry = parseDesktopFile(fileInfo.absoluteFilePath());
        if (entry.isValid) {
            // Don't load icon images immediately - they will be loaded on-demand
            addDesktopEntry(entry);
        }
    }
}

void DesktopDataStore::loadAllDesktopEntries()
{
    // Check if loading is already in progress
    if (m_loadingInProgress) {
        return;
    }
    
    m_loadingInProgress = true;
    
    // Clear existing entries
    clear();
    
    // Start loading in worker thread
    QMetaObject::invokeMethod(m_workerLoader, "loadAllDesktopEntries", Qt::QueuedConnection);
}

void DesktopDataStore::addDesktopEntry(const DesktopEntryData& entryData)
{
    QMutexLocker lock(&m_mutex);
    
    if (entryData.desktopFile.isEmpty() || !entryData.isValid) {
        return;
    }
    
    // Check if this is a new entry or an update
    bool isNewEntry = !m_desktopEntries.contains(entryData.desktopFile);
    
    // Store the entry without loading the icon image immediately
    // Icons will be loaded on-demand when needed
    m_desktopEntries[entryData.desktopFile] = entryData;
    
    // Update language and category lists
    foreach (const QString& lang, entryData.localizedNames.keys()) {
        if (!m_availableLanguages.contains(lang)) {
            m_availableLanguages.append(lang);
        }
    }
    
    foreach (const QString& category, entryData.categories) {
        if (!m_availableCategories.contains(category)) {
            m_availableCategories.append(category);
        }
    }
    
    foreach (const QString& mimeType, entryData.mimeTypes) {
        if (!m_availableMimeTypes.contains(mimeType)) {
            m_availableMimeTypes.append(mimeType);
        }
    }
    
    // Schedule index rebuild
    m_updateTimer->start();
    
    // Only emit signal for new entries to avoid duplicate processing
    if (isNewEntry) {
        emit desktopEntryAdded(entryData);
    } else {
        emit desktopEntryUpdated(entryData);
    }
}

void DesktopDataStore::updateDesktopEntry(const QString& desktopFile, const DesktopEntryData& entryData)
{
    QMutexLocker lock(&m_mutex);
    
    if (!m_desktopEntries.contains(desktopFile)) {
        return;
    }
    
    // Store the entry without loading the icon image immediately
    // Icons will be loaded on-demand when needed
    m_desktopEntries[desktopFile] = entryData;
    
    // Schedule index rebuild
    m_updateTimer->start();
    
    emit desktopEntryUpdated(entryData);
}

void DesktopDataStore::removeDesktopEntry(const QString& desktopFile)
{
    QMutexLocker lock(&m_mutex);
    
    if (m_desktopEntries.contains(desktopFile)) {
        m_desktopEntries.remove(desktopFile);
        
        // Schedule index rebuild
        m_updateTimer->start();
        
        emit desktopEntryRemoved(desktopFile);
    }
}

void DesktopDataStore::clear()
{
    QMutexLocker lock(&m_mutex);
    
    m_desktopEntries.clear();
    m_nameIndex.clear();
    m_iconIndex.clear();
    m_executableIndex.clear();
    m_categoryIndex.clear();
    m_keywordIndex.clear();
    m_mimeTypeIndex.clear();
    m_availableLanguages.clear();
    m_availableCategories.clear();
    m_availableMimeTypes.clear();
    
    emit dataStoreCleared();
}

QList<DesktopEntryData> DesktopDataStore::getAllDesktopEntries(bool includeHidden) const
{
    QMutexLocker lock(&m_mutex);
    
    QList<DesktopEntryData> results;
    
    foreach (const DesktopEntryData& entry, m_desktopEntries) {
        if (!includeHidden && !entry.shouldShow()) continue;
        results.append(entry);
    }
    
    return results;
}

int DesktopDataStore::getDesktopEntryCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_desktopEntries.size();
}

QStringList DesktopDataStore::getAvailableLanguages() const
{
    QMutexLocker lock(&m_mutex);
    return m_availableLanguages;
}

QStringList DesktopDataStore::getAvailableCategories() const
{
    QMutexLocker lock(&m_mutex);
    return m_availableCategories;
}

QStringList DesktopDataStore::getAvailableMimeTypes() const
{
    QMutexLocker lock(&m_mutex);
    return m_availableMimeTypes;
}

void DesktopDataStore::rebuildIndex()
{
    QMutexLocker lock(&m_mutex);
    
    m_nameIndex.clear();
    m_iconIndex.clear();
    m_executableIndex.clear();
    m_categoryIndex.clear();
    m_keywordIndex.clear();
    m_mimeTypeIndex.clear();
    
    // Build indexes
    foreach (const QString& desktopFile, m_desktopEntries.keys()) {
        const DesktopEntryData& entry = m_desktopEntries[desktopFile];
        
        // Index names
        QString normalizedName = normalizeSearchTerm(entry.name);
        if (!normalizedName.isEmpty()) {
            m_nameIndex[normalizedName].append(desktopFile);
        }
        
        // Index icons
        if (!entry.icon.isEmpty()) {
            QString normalizedIcon = normalizeSearchTerm(entry.icon);
            m_iconIndex[normalizedIcon].append(desktopFile);
        }
        
        // Index executables
        if (!entry.exec.isEmpty()) {
            QString normalizedExec = normalizeSearchTerm(entry.exec);
            m_executableIndex[normalizedExec].append(desktopFile);
        }
        
        // Index categories
        foreach (const QString& category, entry.categories) {
            QString normalizedCategory = normalizeSearchTerm(category);
            if (!normalizedCategory.isEmpty()) {
                m_categoryIndex[normalizedCategory].append(desktopFile);
            }
        }
        
        // Index keywords
        foreach (const QString& keyword, entry.keywords) {
            QString normalizedKeyword = normalizeSearchTerm(keyword);
            if (!normalizedKeyword.isEmpty()) {
                m_keywordIndex[normalizedKeyword].append(desktopFile);
            }
        }
        
        // Index MIME types
        foreach (const QString& mimeType, entry.mimeTypes) {
            QString normalizedMimeType = normalizeSearchTerm(mimeType);
            if (!normalizedMimeType.isEmpty()) {
                m_mimeTypeIndex[normalizedMimeType].append(desktopFile);
            }
        }
    }
    
    emit indexRebuilt();
}

QString DesktopDataStore::normalizeSearchTerm(const QString& term) const
{
    if (term.isEmpty()) return QString();
    
    QString normalized = term.toLower().simplified();
    
    // Remove common punctuation and special characters
    normalized.remove(QRegularExpression("[^a-z0-9\\s]"));
    
    // Remove extra whitespace
    normalized = normalized.simplified();
    
    return normalized;
}

// Placeholder implementations for remaining methods
void DesktopDataStore::addDesktopEntries(const QList<DesktopEntryData>& entries) { /* TODO */ }
// updateFromDesktopApplications is no longer needed

void DesktopDataStore::launchApplication(const QString& desktopFile)
{
    DesktopEntryData entry = getDesktopEntry(desktopFile);
    if (!entry.isValid || entry.type != "Application") {
        return;
    }
    
    QString exec = entry.exec;
    
    // Handle special arguments
    for(;;) {
        int argPos = exec.indexOf('%');
        if(argPos == -1)
            break;
        // For now, just remove them
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
    
    // Set working directory if specified
    QString workingDir = entry.path.isEmpty() ? QDir::homePath() : entry.path;
    
    QProcess::startDetached(process, args, workingDir);
}

DesktopApplication DesktopDataStore::convertToDesktopApplication(const DesktopEntryData& entryData) const
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
void DesktopDataStore::optimizeIndex() { /* TODO */ }
QList<DesktopEntryData> DesktopDataStore::searchByName(const QString& name, const QString& language) const { return QList<DesktopEntryData>(); }
QList<DesktopEntryData> DesktopDataStore::searchByIcon(const QString& iconName) const { return QList<DesktopEntryData>(); }
QList<DesktopEntryData> DesktopDataStore::searchByExecutable(const QString& executable) const { return QList<DesktopEntryData>(); }
QList<DesktopEntryData> DesktopDataStore::searchByCategory(const QString& category) const { return QList<DesktopEntryData>(); }
QList<DesktopEntryData> DesktopDataStore::searchByKeywords(const QStringList& keywords) const { return QList<DesktopEntryData>(); }
QList<DesktopEntryData> DesktopDataStore::searchByMimeType(const QString& mimeType) const { return QList<DesktopEntryData>(); }
QList<DesktopEntryData> DesktopDataStore::searchByQuery(const QString& query, const QString& language) const { return QList<DesktopEntryData>(); }
QList<DesktopEntryData> DesktopDataStore::search(const QString& query, const QString& language, const QStringList& categories, bool includeHidden) const { return QList<DesktopEntryData>(); }
DesktopEntryData DesktopDataStore::getDesktopEntry(const QString& desktopFile) const
{
    QMutexLocker lock(&m_mutex);
    
    if (m_desktopEntries.contains(desktopFile)) {
        return m_desktopEntries[desktopFile];
    }
    
    return DesktopEntryData();
}
bool DesktopDataStore::hasDesktopEntry(const QString& desktopFile) const
{
    QMutexLocker lock(&m_mutex);
    return m_desktopEntries.contains(desktopFile);
}
bool DesktopDataStore::fuzzyMatch(const QString& query, const QString& text) const { return false; }
int DesktopDataStore::calculateRelevance(const QString& query, const DesktopEntryData& entry) const { return 0; }

// Worker thread slot methods
void DesktopDataStore::onDesktopEntryLoaded(const DesktopEntryData& entryData)
{
    // This method should only be called from the main thread due to Qt::QueuedConnection
    // Add some debug output to verify thread safety
    addDesktopEntry(entryData);
}

void DesktopDataStore::onLoadingCompleted()
{
    m_loadingInProgress = false;
    
    // Rebuild indexes after all entries are loaded
    rebuildIndex();
}

void DesktopDataStore::onLoadingError(const QString& error)
{
    m_loadingInProgress = false;
}
