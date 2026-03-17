#include "AppPaths.h"
#include <QDir>
#include <QStandardPaths>

namespace {
QString appLocation(QStandardPaths::StandardLocation type) {
    return QStandardPaths::writableLocation(type) + QStringLiteral("/QBrowse");
}
}

QString AppPaths::configDir() { return appLocation(QStandardPaths::AppConfigLocation); }
QString AppPaths::dataDir() { return appLocation(QStandardPaths::AppDataLocation); }
QString AppPaths::cacheDir() { return appLocation(QStandardPaths::CacheLocation); }
QString AppPaths::themesDir() { return dataDir() + QStringLiteral("/themes"); }
QString AppPaths::profileDir() { return dataDir() + QStringLiteral("/profile"); }
QString AppPaths::downloadsDir() {
    const QString p = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return p.isEmpty() ? (QDir::homePath() + QStringLiteral("/Downloads")) : p;
}
QString AppPaths::sessionFile() { return dataDir() + QStringLiteral("/session.json"); }
QString AppPaths::settingsFile() { return configDir() + QStringLiteral("/qbrowse.ini"); }
QString AppPaths::bookmarksFile() { return dataDir() + QStringLiteral("/bookmarks.json"); }
QString AppPaths::historyFile() { return dataDir() + QStringLiteral("/history.json"); }
QString AppPaths::tempDir() { return cacheDir() + QStringLiteral("/tmp"); }

void AppPaths::ensureAll() {
    for (const QString &path : {configDir(), dataDir(), cacheDir(), themesDir(), profileDir(), tempDir()}) {
        QDir().mkpath(path);
    }
}
