#ifndef DESKTOPDATASTORE_H
#define DESKTOPDATASTORE_H

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QHash>
#include <QtGui/QImage>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QRunnable>

// Forward declarations
class DesktopApplication;

struct DesktopEntryData {
    // Basic identification
    QString desktopFile;           // Path to desktop file
    QString type;                  // Application, Link, Directory
    QString version;               // Desktop Entry version
    
    // Names and descriptions
    QString name;                  // Default name
    QMap<QString, QString> localizedNames; // Per-language names (Name[lang])
    QString genericName;           // Generic name
    QMap<QString, QString> localizedGenericNames; // Per-language generic names
    QString comment;               // Comment/description
    QMap<QString, QString> localizedComments; // Per-language comments
    
    // Icons and images
    QString icon;                  // Icon name or path
    QImage iconImage;              // Loaded icon image
    
    // Execution
    QString exec;                  // Executable command
    QString path;                  // Working directory
    bool terminal;                 // Whether to run in terminal
    QStringList actions;           // Action names
    
    // Categories and keywords
    QStringList categories;        // Application categories
    QStringList keywords;          // Keywords for search
    QStringList onlyShowIn;        // Desktop environments to show in
    QStringList notShowIn;         // Desktop environments to hide in
    
    // Display and behavior
    bool noDisplay;                // Whether to hide from menus
    bool hidden;                   // Whether entry is hidden
    bool startupNotify;            // Whether to show startup notification
    QString startupWMClass;        // WM class for startup notification
    
    // MIME types and URLs
    QStringList mimeTypes;         // Supported MIME types
    QString url;                   // URL for Link type entries
    
    // Validation
    bool isValid;                  // Whether this entry is valid
    QStringList errors;            // Parsing errors
    
    DesktopEntryData() : terminal(false), noDisplay(false), hidden(false), 
                        startupNotify(false), isValid(false) {}
    
    // Get localized name for a specific language
    QString getLocalizedName(const QString& language = QString()) const;
    QString getLocalizedGenericName(const QString& language = QString()) const;
    QString getLocalizedComment(const QString& language = QString()) const;
    
    // Get all searchable text (names, keywords, etc.)
    QStringList getAllSearchableText() const;
    
    // Check if this entry matches a search query
    bool matchesQuery(const QString& query) const;
    
    // Check if entry should be shown in current environment
    bool shouldShow() const;
    
    // Get display name (prefer generic name if available)
    QString getDisplayName(const QString& language = QString()) const;
};

// Worker class for loading desktop entries in a separate thread
class DesktopEntryLoader : public QObject
{
    Q_OBJECT

public:
    explicit DesktopEntryLoader(QObject* parent = nullptr);
    ~DesktopEntryLoader();

public slots:
    void loadAllDesktopEntries();

signals:
    void desktopEntryLoaded(const DesktopEntryData& entryData);
    void loadingCompleted();
    void loadingError(const QString& error);

private:
    DesktopEntryData parseDesktopFile(const QString& filePath);
    void loadDesktopEntriesFromDirectory(const QString& directoryPath);
    QStringList getSearchPaths() const;
};

class DesktopDataStore : public QObject
{
    Q_OBJECT

public:
    static DesktopDataStore* instance();
    
    // Data management
    void addDesktopEntry(const DesktopEntryData& entryData);
    void updateDesktopEntry(const QString& desktopFile, const DesktopEntryData& entryData);
    void removeDesktopEntry(const QString& desktopFile);
    void clear();
    
    // Desktop entry parsing
    DesktopEntryData parseDesktopFile(const QString& filePath);
    void loadDesktopEntriesFromDirectory(const QString& directory);
    void loadAllDesktopEntries();
    
    // Search methods
    QList<DesktopEntryData> searchByName(const QString& name, const QString& language = QString()) const;
    QList<DesktopEntryData> searchByIcon(const QString& iconName) const;
    QList<DesktopEntryData> searchByExecutable(const QString& executable) const;
    QList<DesktopEntryData> searchByCategory(const QString& category) const;
    QList<DesktopEntryData> searchByKeywords(const QStringList& keywords) const;
    QList<DesktopEntryData> searchByMimeType(const QString& mimeType) const;
    QList<DesktopEntryData> searchByQuery(const QString& query, const QString& language = QString()) const;
    
    // Advanced search with filters
    QList<DesktopEntryData> search(const QString& query, 
                                  const QString& language = QString(),
                                  const QStringList& categories = QStringList(),
                                  bool includeHidden = false) const;
    
    // Get all desktop entries
    QList<DesktopEntryData> getAllDesktopEntries(bool includeHidden = false) const;
    
    // Get specific desktop entry
    DesktopEntryData getDesktopEntry(const QString& desktopFile) const;
    bool hasDesktopEntry(const QString& desktopFile) const;
    
    // Application launching
    void launchApplication(const QString& desktopFile);
    
    // Convert DesktopEntryData to DesktopApplication (for compatibility)
    DesktopApplication convertToDesktopApplication(const DesktopEntryData& entryData) const;
    
    // Statistics
    int getDesktopEntryCount() const;
    QStringList getAvailableLanguages() const;
    QStringList getAvailableCategories() const;
    QStringList getAvailableMimeTypes() const;
    
    // Bulk operations
    void addDesktopEntries(const QList<DesktopEntryData>& entries);
    // updateFromDesktopApplications is no longer needed
    
    // Index management
    void rebuildIndex();
    void optimizeIndex();

signals:
    void desktopEntryAdded(const DesktopEntryData& entryData);
    void desktopEntryUpdated(const DesktopEntryData& entryData);
    void desktopEntryRemoved(const QString& desktopFile);
    void dataStoreCleared();
    void indexRebuilt();

private slots:
    void onDesktopEntryLoaded(const DesktopEntryData& entryData);
    void onLoadingCompleted();
    void onLoadingError(const QString& error);

private:
    explicit DesktopDataStore(QObject* parent = nullptr);
    ~DesktopDataStore();
    Q_DISABLE_COPY(DesktopDataStore)
    
    // Index building
    void buildNameIndex();
    void buildIconIndex();
    void buildWmClassIndex();
    void buildExecutableIndex();
    void buildCategoryIndex();
    void buildKeywordIndex();
    
    // Search helpers
    QString normalizeSearchTerm(const QString& term) const;
    bool fuzzyMatch(const QString& query, const QString& text) const;
    int calculateRelevance(const QString& query, const DesktopEntryData& entry) const;
    
    // Data storage
    QMap<QString, DesktopEntryData> m_desktopEntries;
    mutable QMutex m_mutex;
    
    // Search indexes
    QMap<QString, QStringList> m_nameIndex;        // normalized name -> desktop files
    QMap<QString, QStringList> m_iconIndex;        // icon name -> desktop files
    QMap<QString, QStringList> m_executableIndex;  // executable -> desktop files
    QMap<QString, QStringList> m_categoryIndex;    // category -> desktop files
    QMap<QString, QStringList> m_keywordIndex;     // keyword -> desktop files
    QMap<QString, QStringList> m_mimeTypeIndex;    // mime type -> desktop files
    
    // Cached data
    QStringList m_availableLanguages;
    QStringList m_availableCategories;
    QStringList m_availableMimeTypes;
    
    // Auto-update timer
    QTimer* m_updateTimer;
    
    // Threading support
    QThread* m_workerThread;
    DesktopEntryLoader* m_workerLoader;
    bool m_loadingInProgress;
    
    static DesktopDataStore* m_instance;
};

#endif // DESKTOPDATASTORE_H
