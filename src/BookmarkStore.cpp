#include "BookmarkStore.h"
#include "AppPaths.h"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

BookmarkStore::BookmarkStore(QObject *parent) : QObject(parent) { AppPaths::ensureAll(); load(); }
QVector<BookmarkEntry> BookmarkStore::entries() const { return m_entries; }

QStringList BookmarkStore::folders() const {
    QSet<QString> set;
    for (const auto &e : m_entries) if (!e.folder.trimmed().isEmpty()) set.insert(e.folder.trimmed());
    QStringList out = set.values();
    out.sort(Qt::CaseInsensitive);
    return out;
}

bool BookmarkStore::addBookmark(const QString &title, const QString &url, const QString &folder, const QStringList &tags) {
    const QString normalized = url.trimmed();
    if (normalized.isEmpty()) return false;
    for (const auto &e : m_entries) {
        if (e.url.compare(normalized, Qt::CaseInsensitive) == 0) return false;
    }
    BookmarkEntry entry;
    entry.title = title.trimmed().isEmpty() ? normalized : title.trimmed();
    entry.url = normalized;
    entry.folder = folder.trimmed();
    entry.tags = tags;
    entry.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m_entries.push_back(entry);
    save();
    emit changed();
    return true;
}

void BookmarkStore::addBookmarks(const QVector<BookmarkEntry> &entries) {
    for (const auto &e : entries) addBookmark(e.title, e.url, e.folder, e.tags);
}

void BookmarkStore::updateAt(int index, const BookmarkEntry &entry) {
    if (index < 0 || index >= m_entries.size()) return;
    m_entries[index] = entry;
    save();
    emit changed();
}

void BookmarkStore::removeAt(int index) {
    if (index < 0 || index >= m_entries.size()) return;
    m_entries.removeAt(index);
    save();
    emit changed();
}

void BookmarkStore::clear() { m_entries.clear(); save(); emit changed(); }

QVector<BookmarkEntry> BookmarkStore::search(const QString &needle) const {
    const QString n = needle.trimmed().toLower();
    if (n.isEmpty()) return m_entries;
    QVector<BookmarkEntry> out;
    for (const auto &e : m_entries) {
        const QString hay = (e.title + QStringLiteral(" ") + e.url + QStringLiteral(" ") + e.folder + QStringLiteral(" ") + e.tags.join(QStringLiteral(" "))).toLower();
        if (hay.contains(n)) out.push_back(e);
    }
    return out;
}

bool BookmarkStore::exportJson(const QString &path) const {
    QJsonArray array;
    for (const auto &e : m_entries) {
        QJsonObject o;
        o.insert("title", e.title);
        o.insert("url", e.url);
        o.insert("createdAt", e.createdAt);
        o.insert("folder", e.folder);
        o.insert("pinned", e.pinned);
        o.insert("tags", QJsonArray::fromStringList(e.tags));
        array.push_back(o);
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    return true;
}

bool BookmarkStore::importJson(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return false;
    for (const auto &v : doc.array()) {
        const auto o = v.toObject();
        BookmarkEntry e;
        e.title = o.value("title").toString();
        e.url = o.value("url").toString();
        e.createdAt = o.value("createdAt").toString();
        e.folder = o.value("folder").toString();
        e.pinned = o.value("pinned").toBool();
        for (const auto &t : o.value("tags").toArray()) e.tags.push_back(t.toString());
        addBookmark(e.title, e.url, e.folder, e.tags);
    }
    return true;
}

bool BookmarkStore::exportNetscapeHtml(const QString &path) const {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    QTextStream out(&f);
    out << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n";
    out << "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n";
    out << "<TITLE>QBrowse Bookmarks</TITLE>\n<H1>QBrowse Bookmarks</H1>\n<DL><p>\n";
    QString currentFolder;
    for (const auto &e : m_entries) {
        if (e.folder != currentFolder) {
            if (!currentFolder.isEmpty()) out << "</DL><p>\n";
            currentFolder = e.folder;
            if (!currentFolder.isEmpty()) out << "<DT><H3>" << currentFolder.toHtmlEscaped() << "</H3>\n<DL><p>\n";
        }
        out << "<DT><A HREF=\"" << e.url.toHtmlEscaped() << "\">" << e.title.toHtmlEscaped() << "</A>\n";
    }
    if (!currentFolder.isEmpty()) out << "</DL><p>\n";
    out << "</DL><p>\n";
    return true;
}

void BookmarkStore::load() {
    QFile f(AppPaths::bookmarksFile());
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return;
    m_entries.clear();
    for (const auto &v : doc.array()) {
        const auto o = v.toObject();
        BookmarkEntry e;
        e.title = o.value("title").toString();
        e.url = o.value("url").toString();
        e.createdAt = o.value("createdAt").toString();
        e.folder = o.value("folder").toString();
        e.pinned = o.value("pinned").toBool();
        for (const auto &t : o.value("tags").toArray()) e.tags.push_back(t.toString());
        if (!e.url.isEmpty()) m_entries.push_back(e);
    }
}

void BookmarkStore::save() const { exportJson(AppPaths::bookmarksFile()); }
