#include "ImportManager.h"
#include "BookmarkStore.h"
#include "HistoryStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimeZone>
#include <QUuid>

#include <algorithm>

namespace {
QString tempCopyOf(const QString &sourcePath, QTemporaryDir &tempDir) {
    QFileInfo info(sourcePath);
    const QString tempPath = tempDir.path() + QStringLiteral("/") + info.fileName();
    QFile::remove(tempPath);
    QFile::copy(sourcePath, tempPath);
    return tempPath;
}

QSqlDatabase openSqlite(const QString &path, const QString &connectionName) {
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(path);
    db.open();
    return db;
}

QDateTime firefoxMicrosToDateTime(qint64 micros) {
    return QDateTime::fromMSecsSinceEpoch(micros / 1000, QTimeZone::UTC);
}

QDateTime chromiumTimeToDateTime(qint64 microsSince1601) {
    static constexpr qint64 epochDeltaMicros = 11644473600000000LL;
    return QDateTime::fromMSecsSinceEpoch((microsSince1601 - epochDeltaMicros) / 1000, QTimeZone::UTC);
}

void parseChromiumBookmarkNode(const QJsonObject &node, QVector<BookmarkEntry> &out) {
    const QString type = node.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("url")) {
        BookmarkEntry entry;
        entry.title = node.value(QStringLiteral("name")).toString();
        entry.url = node.value(QStringLiteral("url")).toString();
        entry.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!entry.url.isEmpty()) {
            out.push_back(entry);
        }
        return;
    }

    const auto children = node.value(QStringLiteral("children")).toArray();
    for (const auto &child : children) {
        parseChromiumBookmarkNode(child.toObject(), out);
    }
}

QString profileFromIni(const QString &iniPath, const QString &basePrefix) {
    if (!QFile::exists(iniPath)) {
        return {};
    }

    QSettings ini(iniPath, QSettings::IniFormat);
    const auto groups = ini.childGroups();

    QString selectedPath;
    for (const auto &group : groups) {
        ini.beginGroup(group);
        const bool isDefault = ini.value(QStringLiteral("Default"), 0).toInt() == 1;
        const QString path = ini.value(QStringLiteral("Path")).toString();
        const bool isRelative = ini.value(QStringLiteral("IsRelative"), 1).toInt() == 1;
        ini.endGroup();

        if (!path.isEmpty() && isDefault) {
            if (isRelative) {
                return basePrefix + path;
            }
            return path;
        }
        if (!path.isEmpty() && selectedPath.isEmpty()) {
            selectedPath = isRelative ? (basePrefix + path) : path;
        }
    }
    return selectedPath;
}

QString browserName(BrowserSource source) {
    switch (source) {
    case BrowserSource::Firefox: return QStringLiteral("Firefox");
    case BrowserSource::Floorp: return QStringLiteral("Floorp");
    case BrowserSource::Chromium: return QStringLiteral("Chromium");
    case BrowserSource::Chrome: return QStringLiteral("Google Chrome");
    case BrowserSource::Brave: return QStringLiteral("Brave");
    case BrowserSource::Vivaldi: return QStringLiteral("Vivaldi");
    case BrowserSource::Opera: return QStringLiteral("Opera");
    case BrowserSource::Edge: return QStringLiteral("Microsoft Edge");
    }
    return QStringLiteral("Unknown browser");
}
}

QString ImportReport::summary() const {
    QString text = QStringLiteral("Bookmarks: %1\nHistory: %2").arg(bookmarksImported).arg(historyImported);
    if (!notes.isEmpty()) {
        text += QStringLiteral("\n\nNotes:\n- ") + notes.join(QStringLiteral("\n- "));
    }
    return text;
}

ImportManager::ImportManager(BookmarkStore *bookmarkStore, HistoryStore *historyStore, QObject *parent)
    : QObject(parent)
    , m_bookmarkStore(bookmarkStore)
    , m_historyStore(historyStore) {
}

QString ImportManager::firefoxProfilePath() const {
    return profileFromIni(QDir::homePath() + QStringLiteral("/.mozilla/firefox/profiles.ini"),
                          QDir::homePath() + QStringLiteral("/.mozilla/firefox/"));
}

QString ImportManager::floorpProfilePath() const {
    const QString direct = profileFromIni(QDir::homePath() + QStringLiteral("/.floorp/profiles.ini"),
                                          QDir::homePath() + QStringLiteral("/.floorp/"));
    if (!direct.isEmpty()) {
        return direct;
    }
    return profileFromIni(QDir::homePath() + QStringLiteral("/.var/app/one.ablaze.floorp/.floorp/profiles.ini"),
                          QDir::homePath() + QStringLiteral("/.var/app/one.ablaze.floorp/.floorp/"));
}

QString ImportManager::chromiumProfilePath(BrowserSource source) const {
    const QString home = QDir::homePath();
    switch (source) {
    case BrowserSource::Chromium:
        return home + QStringLiteral("/.config/chromium/Default");
    case BrowserSource::Chrome:
        return home + QStringLiteral("/.config/google-chrome/Default");
    case BrowserSource::Brave:
        return home + QStringLiteral("/.config/BraveSoftware/Brave-Browser/Default");
    case BrowserSource::Vivaldi:
        return home + QStringLiteral("/.config/vivaldi/Default");
    case BrowserSource::Opera:
        return home + QStringLiteral("/.config/opera/Default");
    case BrowserSource::Edge:
        return home + QStringLiteral("/.config/microsoft-edge/Default");
    case BrowserSource::Firefox:
        return firefoxProfilePath();
    case BrowserSource::Floorp:
        return floorpProfilePath();
    }
    return {};
}

ImportReport ImportManager::importFirefox() {
    return importFirefoxProfile(firefoxProfilePath(), QStringLiteral("Firefox"));
}

ImportReport ImportManager::importBrowser(BrowserSource source) {
    if (source == BrowserSource::Firefox) {
        return importFirefox();
    }
    if (source == BrowserSource::Floorp) {
        return importFirefoxProfile(floorpProfilePath(), QStringLiteral("Floorp"));
    }
    return importChromiumProfile(chromiumProfilePath(source), source);
}

ImportReport ImportManager::importAuto() {
    for (const auto source : {BrowserSource::Firefox,
                              BrowserSource::Floorp,
                              BrowserSource::Chromium,
                              BrowserSource::Chrome,
                              BrowserSource::Brave,
                              BrowserSource::Vivaldi,
                              BrowserSource::Opera,
                              BrowserSource::Edge}) {
        ImportReport report = importBrowser(source);
        if (report.success) {
            report.notes.prepend(QStringLiteral("Auto-detected browser import completed."));
            return report;
        }
    }
    ImportReport report;
    report.success = false;
    report.notes << QStringLiteral("No supported browser profiles were found.");
    return report;
}

ImportReport ImportManager::importFirefoxProfile(const QString &profilePath, const QString &sourceName) {
    ImportReport report;
    if (profilePath.isEmpty() || !QDir(profilePath).exists()) {
        report.notes << QStringLiteral("%1 profile not found.").arg(sourceName);
        return report;
    }

    const QString placesPath = profilePath + QStringLiteral("/places.sqlite");
    if (!QFile::exists(placesPath)) {
        report.notes << QStringLiteral("%1 places.sqlite not found.").arg(sourceName);
        return report;
    }

    QTemporaryDir tempDir;
    const QString dbPath = tempCopyOf(placesPath, tempDir);
    const QString connection = QStringLiteral("import-%1-%2").arg(sourceName.toLower(), QUuid::createUuid().toString(QUuid::WithoutBraces));
    auto db = openSqlite(dbPath, connection);

    QVector<BookmarkEntry> bookmarks;
    {
        QSqlQuery query(db);
        query.exec(QStringLiteral(
            "SELECT p.url, COALESCE(b.title, p.title, p.url) "
            "FROM moz_bookmarks b "
            "JOIN moz_places p ON p.id = b.fk "
            "WHERE b.type = 1 AND p.url IS NOT NULL"));
        while (query.next()) {
            BookmarkEntry entry;
            entry.url = query.value(0).toString();
            entry.title = query.value(1).toString();
            entry.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            if (!entry.url.isEmpty()) {
                bookmarks.push_back(entry);
            }
        }
    }

    const int before = m_bookmarkStore->entries().size();
    m_bookmarkStore->addBookmarks(bookmarks);
    report.bookmarksImported = m_bookmarkStore->entries().size() - before;

    {
        QSqlQuery query(db);
        query.exec(QStringLiteral(
            "SELECT url, COALESCE(title, url), last_visit_date, visit_count "
            "FROM moz_places WHERE hidden = 0 AND last_visit_date IS NOT NULL "
            "ORDER BY last_visit_date DESC LIMIT 2000"));
        while (query.next()) {
            const QString url = query.value(0).toString();
            const QString title = query.value(1).toString();
            const qint64 lastVisitDate = query.value(2).toLongLong();
            const int visitCount = query.value(3).toInt();

            if (url.isEmpty()) {
                continue;
            }
            for (int i = 0; i < std::max(1, std::min(visitCount, 3)); ++i) {
                m_historyStore->addVisit(title, url, firefoxMicrosToDateTime(lastVisitDate));
                ++report.historyImported;
            }
        }
    }

    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection);

    report.success = report.bookmarksImported > 0 || report.historyImported > 0;
    report.notes << QStringLiteral("Imported from %1 profile: %2").arg(sourceName, profilePath);
    report.notes << QStringLiteral("Passwords/cookies are intentionally not imported in this starter code.");
    return report;
}

ImportReport ImportManager::importChromiumProfile(const QString &profilePath, BrowserSource source) {
    ImportReport report;
    if (profilePath.isEmpty() || !QDir(profilePath).exists()) {
        report.notes << QStringLiteral("%1 profile not found.").arg(browserName(source));
        return report;
    }

    const QString bookmarksPath = profilePath + QStringLiteral("/Bookmarks");
    if (QFile::exists(bookmarksPath)) {
        QFile file(bookmarksPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const auto doc = QJsonDocument::fromJson(file.readAll());
            const auto roots = doc.object().value(QStringLiteral("roots")).toObject();

            QVector<BookmarkEntry> bookmarks;
            for (const auto &key : roots.keys()) {
                parseChromiumBookmarkNode(roots.value(key).toObject(), bookmarks);
            }

            const int before = m_bookmarkStore->entries().size();
            m_bookmarkStore->addBookmarks(bookmarks);
            report.bookmarksImported = m_bookmarkStore->entries().size() - before;
        }
    }

    const QString historyPath = profilePath + QStringLiteral("/History");
    if (QFile::exists(historyPath)) {
        QTemporaryDir tempDir;
        const QString dbPath = tempCopyOf(historyPath, tempDir);
        const QString connection = QStringLiteral("import-%1-%2").arg(browserName(source).toLower().replace(' ', '-'), QUuid::createUuid().toString(QUuid::WithoutBraces));
        auto db = openSqlite(dbPath, connection);

        QSqlQuery query(db);
        query.exec(QStringLiteral(
            "SELECT url, COALESCE(title, url), last_visit_time, visit_count "
            "FROM urls ORDER BY last_visit_time DESC LIMIT 2000"));
        while (query.next()) {
            const QString url = query.value(0).toString();
            const QString title = query.value(1).toString();
            const qint64 lastVisitDate = query.value(2).toLongLong();
            const int visitCount = query.value(3).toInt();

            if (url.isEmpty()) {
                continue;
            }
            for (int i = 0; i < std::max(1, std::min(visitCount, 3)); ++i) {
                m_historyStore->addVisit(title, url, chromiumTimeToDateTime(lastVisitDate));
                ++report.historyImported;
            }
        }

        db.close();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(connection);
    }

    report.success = report.bookmarksImported > 0 || report.historyImported > 0;
    report.notes << QStringLiteral("Imported from %1 profile: %2").arg(browserName(source), profilePath);
    report.notes << QStringLiteral("Passwords/cookies are intentionally not imported in this starter code.");
    return report;
}
