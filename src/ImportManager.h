#pragma once
#include <QObject>
#include <QStringList>

class BookmarkStore;
class HistoryStore;

enum class BrowserSource {
    Firefox,
    Floorp,
    Chromium,
    Chrome,
    Brave,
    Vivaldi,
    Opera,
    Edge
};

struct ImportReport {
    bool success = false;
    int bookmarksImported = 0;
    int historyImported = 0;
    QStringList notes;
    QString summary() const;
};

class ImportManager : public QObject {
    Q_OBJECT
public:
    explicit ImportManager(BookmarkStore *bookmarkStore, HistoryStore *historyStore, QObject *parent = nullptr);
    QString firefoxProfilePath() const;
    QString floorpProfilePath() const;
    QString chromiumProfilePath(BrowserSource source) const;
    ImportReport importFirefox();
    ImportReport importBrowser(BrowserSource source);
    ImportReport importAuto();
private:
    ImportReport importFirefoxProfile(const QString &profilePath, const QString &sourceName);
    ImportReport importChromiumProfile(const QString &profilePath, BrowserSource source);
    BookmarkStore *m_bookmarkStore;
    HistoryStore *m_historyStore;
};
