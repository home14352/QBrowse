#pragma once
#include <QObject>
#include <QDateTime>
#include <QVector>

struct HistoryEntry {
    QString title;
    QString url;
    QDateTime visitedAt;
    int visitCount = 0;
};

class HistoryStore : public QObject {
    Q_OBJECT
public:
    explicit HistoryStore(QObject *parent = nullptr);
    void addVisit(const QString &title, const QString &url, const QDateTime &when = QDateTime::currentDateTimeUtc());
    QVector<HistoryEntry> recent(int limit = 500) const;
    QVector<HistoryEntry> search(const QString &needle) const;
    int totalEntries() const;
    void clear();
signals:
    void changed();
private:
    void load();
    void save() const;
    QVector<HistoryEntry> m_entries;
};
