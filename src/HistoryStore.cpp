#include "HistoryStore.h"
#include "AppPaths.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

HistoryStore::HistoryStore(QObject *parent) : QObject(parent) { AppPaths::ensureAll(); load(); }

void HistoryStore::addVisit(const QString &title, const QString &url, const QDateTime &when) {
    if (url.trimmed().isEmpty()) return;
    for (auto &e : m_entries) {
        if (e.url == url) {
            e.title = title.isEmpty() ? e.title : title;
            e.visitedAt = when;
            e.visitCount += 1;
            save();
            emit changed();
            return;
        }
    }
    HistoryEntry e;
    e.title = title.isEmpty() ? url : title;
    e.url = url;
    e.visitedAt = when;
    e.visitCount = 1;
    m_entries.push_back(e);
    save();
    emit changed();
}

QVector<HistoryEntry> HistoryStore::recent(int limit) const {
    QVector<HistoryEntry> copy = m_entries;
    std::sort(copy.begin(), copy.end(), [](const HistoryEntry &a, const HistoryEntry &b) {
        return a.visitedAt > b.visitedAt;
    });
    if (limit > 0 && copy.size() > limit) copy.resize(limit);
    return copy;
}

QVector<HistoryEntry> HistoryStore::search(const QString &needle) const {
    const QString n = needle.trimmed().toLower();
    if (n.isEmpty()) return recent();
    QVector<HistoryEntry> out;
    for (const auto &e : m_entries) {
        const QString hay = (e.title + QStringLiteral(" ") + e.url).toLower();
        if (hay.contains(n)) out.push_back(e);
    }
    std::sort(out.begin(), out.end(), [](const HistoryEntry &a, const HistoryEntry &b) {
        return a.visitedAt > b.visitedAt;
    });
    return out;
}

int HistoryStore::totalEntries() const { return m_entries.size(); }
void HistoryStore::clear() { m_entries.clear(); save(); emit changed(); }

void HistoryStore::load() {
    QFile f(AppPaths::historyFile());
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return;
    for (const auto &v : doc.array()) {
        const auto o = v.toObject();
        HistoryEntry e;
        e.title = o.value("title").toString();
        e.url = o.value("url").toString();
        e.visitedAt = QDateTime::fromString(o.value("visitedAt").toString(), Qt::ISODate);
        e.visitCount = o.value("visitCount").toInt();
        if (!e.url.isEmpty()) m_entries.push_back(e);
    }
}

void HistoryStore::save() const {
    QJsonArray arr;
    for (const auto &e : m_entries) {
        QJsonObject o;
        o.insert("title", e.title);
        o.insert("url", e.url);
        o.insert("visitedAt", e.visitedAt.toString(Qt::ISODate));
        o.insert("visitCount", e.visitCount);
        arr.push_back(o);
    }
    QFile f(AppPaths::historyFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}
