#pragma once
#include <QColor>
#include <QVector>

class QApplication;
class SettingsManager;

struct InstalledTheme {
    QString id;
    QString title;
    QString source;
    QString rootDir;
    QString backgroundImage;
    QString animatedAsset;
    QColor frameColor;
    QColor toolbarColor;
    QColor tabTextColor;
    QColor textColor;
    QColor accentColor;
    bool animated = false;
};

class ThemeManager {
public:
    static void apply(QApplication &app, const SettingsManager &settings);
    static QVector<InstalledTheme> installedThemes();
    static InstalledTheme activeTheme(const SettingsManager &settings);
    static QString currentAnimatedAsset(const SettingsManager &settings);
    static bool installThemePackage(const QString &path, const QString &source, QString *message = nullptr);
    static bool extractArchive(const QString &archivePath, const QString &targetDir);
};
