#pragma once
#include <QObject>
#include <QStringList>
#include <QVector>

struct BookmarkEntry {
    QString title;
    QString url;
    QString createdAt;
    QString folder;
    QStringList tags;
    bool pinned = false;
};

class BookmarkStore : public QObject {
    Q_OBJECT
public:
    explicit BookmarkStore(QObject *parent = nullptr);
    QVector<BookmarkEntry> entries() const;
    QStringList folders() const;
    bool addBookmark(const QString &title, const QString &url, const QString &folder = {}, const QStringList &tags = {});
    void addBookmarks(const QVector<BookmarkEntry> &entries);
    void updateAt(int index, const BookmarkEntry &entry);
    void removeAt(int index);
    void clear();
    QVector<BookmarkEntry> search(const QString &needle) const;
    bool exportJson(const QString &path) const;
    bool importJson(const QString &path);
    bool exportNetscapeHtml(const QString &path) const;
signals:
    void changed();
private:
    void load();
    void save() const;
    QVector<BookmarkEntry> m_entries;
};
