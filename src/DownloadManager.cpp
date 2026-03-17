#include "DownloadManager.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>
#include <QWebEngineDownloadRequest>

DownloadManager::DownloadManager(QObject *parent) : QObject(parent) {}
QVector<DownloadEntry> DownloadManager::entries() const { return m_entries; }

void DownloadManager::handleDownload(QWebEngineDownloadRequest *request) {
    if (!request) return;
    DownloadEntry entry;
    entry.fileName = QFileInfo(request->downloadFileName()).fileName();
    entry.path = request->downloadDirectory() + QStringLiteral("/") + request->downloadFileName();
    entry.sourceUrl = request->url().toString();
    entry.state = QStringLiteral("Загрузка");
    m_entries.push_back(entry);
    const int index = m_entries.size() - 1;
    request->accept();
    connect(request, &QWebEngineDownloadRequest::receivedBytesChanged, this, [this, request, index]() {
        if (index >= 0 && index < m_entries.size()) {
            const qint64 total = request->totalBytes();
            m_entries[index].progress = total > 0 ? int((100.0 * request->receivedBytes()) / total) : 0;
            emit changed();
        }
    });
    connect(request, &QWebEngineDownloadRequest::stateChanged, this, [this, request, index](QWebEngineDownloadRequest::DownloadState state) {
        if (index < 0 || index >= m_entries.size()) return;
        switch (state) {
        case QWebEngineDownloadRequest::DownloadCompleted:
            m_entries[index].state = QStringLiteral("Готово");
            m_entries[index].progress = 100;
            break;
        case QWebEngineDownloadRequest::DownloadInterrupted:
            m_entries[index].state = QStringLiteral("Прервано");
            break;
        case QWebEngineDownloadRequest::DownloadCancelled:
            m_entries[index].state = QStringLiteral("Отменено");
            break;
        default:
            m_entries[index].state = QStringLiteral("Загрузка");
            break;
        }
        emit changed();
    });
    emit changed();
}

bool DownloadManager::openPath(int index) const {
    if (index < 0 || index >= m_entries.size()) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(m_entries[index].path));
}
