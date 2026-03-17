#pragma once
#include <QObject>
#include <QVector>

class QWebEngineDownloadRequest;

struct DownloadEntry {
    QString fileName;
    QString path;
    QString sourceUrl;
    QString state;
    int progress = 0;
};

class DownloadManager : public QObject {
    Q_OBJECT
public:
    explicit DownloadManager(QObject *parent = nullptr);
    QVector<DownloadEntry> entries() const;
    void handleDownload(QWebEngineDownloadRequest *request);
    bool openPath(int index) const;
signals:
    void changed();
private:
    QVector<DownloadEntry> m_entries;
};
