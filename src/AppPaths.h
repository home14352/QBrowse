#pragma once
#include <QString>

class AppPaths {
public:
    static QString configDir();
    static QString dataDir();
    static QString cacheDir();
    static QString themesDir();
    static QString profileDir();
    static QString downloadsDir();
    static QString sessionFile();
    static QString settingsFile();
    static QString bookmarksFile();
    static QString historyFile();
    static QString tempDir();
    static void ensureAll();
};
