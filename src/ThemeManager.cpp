#include "ThemeManager.h"
#include "AppPaths.h"
#include "SettingsManager.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QProcess>
#include <QRegularExpression>
#include <QStyleFactory>
#include <QUuid>
#include <QFileInfo>


namespace {
QColor parseColor(const QJsonValue &value) {
    if (value.isArray()) {
        const auto arr = value.toArray();
        if (arr.size() >= 3) return QColor(arr.at(0).toInt(), arr.at(1).toInt(), arr.at(2).toInt());
    }
    if (value.isString()) return QColor(value.toString());
    return {};
}

InstalledTheme fromRoot(const QString &rootDir) {
    InstalledTheme theme;
    QFile manifestFile(rootDir + QStringLiteral("/manifest.json"));
    if (!manifestFile.open(QIODevice::ReadOnly)) return theme;
    const auto manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    const auto themeObject = manifest.value(QStringLiteral("theme")).toObject();
    if (themeObject.isEmpty()) return theme;

    theme.id = QFileInfo(rootDir).fileName();
    theme.title = manifest.value(QStringLiteral("name")).toString(theme.id);
    theme.rootDir = rootDir;
    theme.source = QFile(rootDir + QStringLiteral("/.source")).exists() ? QString() : QStringLiteral("local");

    const QJsonObject colors = themeObject.value(QStringLiteral("colors")).toObject();
    const QJsonObject images = themeObject.value(QStringLiteral("images")).toObject();
    theme.frameColor = parseColor(colors.value(QStringLiteral("frame")));
    theme.toolbarColor = parseColor(colors.value(QStringLiteral("toolbar")));
    theme.tabTextColor = parseColor(colors.value(QStringLiteral("tab_text")));
    if (!theme.tabTextColor.isValid()) theme.tabTextColor = parseColor(colors.value(QStringLiteral("tab_background_text")));
    theme.textColor = parseColor(colors.value(QStringLiteral("toolbar_field_text")));
    theme.accentColor = parseColor(colors.value(QStringLiteral("icons")));
    if (!theme.accentColor.isValid()) theme.accentColor = parseColor(colors.value(QStringLiteral("bookmark_text")));

    QString imagePath = images.value(QStringLiteral("theme_frame")).toString();
    if (imagePath.isEmpty()) {
        const auto additional = images.value(QStringLiteral("additional_backgrounds")).toArray();
        if (!additional.isEmpty()) imagePath = additional.first().toString();
    }
    if (!imagePath.isEmpty()) {
        const QString resolved = QDir(rootDir).absoluteFilePath(imagePath);
        theme.backgroundImage = resolved;
        QImageReader reader(resolved);
        if (reader.supportsAnimation()) {
            theme.animated = true;
            theme.animatedAsset = resolved;
        } else if (resolved.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
            theme.animated = true;
            theme.animatedAsset = resolved;
        }
    }
    return theme;
}

QString presetStyle(const QPalette &base, const SettingsManager &settings, const InstalledTheme &theme) {
    QColor window = base.window().color();
    QColor baseColor = base.base().color();
    QColor text = base.text().color();
    QColor highlight = base.highlight().color();

    const QString preset = settings.stringValue(QStringLiteral("appearance/preset"), QStringLiteral("system"));
    if (preset == QStringLiteral("dark")) {
        window = QColor(QStringLiteral("#121826"));
        baseColor = QColor(QStringLiteral("#111827"));
        text = QColor(QStringLiteral("#f3f4f6"));
        highlight = QColor(QStringLiteral("#5b8cff"));
    } else if (preset == QStringLiteral("plasma")) {
        window = QColor(QStringLiteral("#1c2434"));
        baseColor = QColor(QStringLiteral("#111827"));
        text = QColor(QStringLiteral("#e5eefb"));
        highlight = QColor(QStringLiteral("#4f8cff"));
    } else if (preset == QStringLiteral("gnome")) {
        window = QColor(QStringLiteral("#202226"));
        baseColor = QColor(QStringLiteral("#17191c"));
        text = QColor(QStringLiteral("#f7f7f7"));
        highlight = QColor(QStringLiteral("#3584e4"));
    }

    if (!theme.id.isEmpty()) {
        if (theme.frameColor.isValid()) window = theme.frameColor;
        if (theme.toolbarColor.isValid()) baseColor = theme.toolbarColor;
        if (theme.tabTextColor.isValid()) text = theme.tabTextColor;
        if (theme.accentColor.isValid()) highlight = theme.accentColor;
    }

    const int radius = settings.boolValue(QStringLiteral("appearance/compactMode"), false) ? 8 : 12;
    return QStringLiteral(R"(
        QMainWindow, QDialog, QWidget { background:%1; color:%2; }
        QToolBar, QMenuBar, QStatusBar { background:%3; border:0; spacing:4px; }
        QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QTreeWidget, QTableWidget, QListWidget {
            background:%3; color:%2; border:1px solid rgba(255,255,255,0.08); border-radius:%4px; padding:6px;
        }
        QTabWidget::pane { border:0; }
        QTabBar::tab { background:%3; color:%2; padding:8px 10px; margin:2px; border-radius:%4px; }
        QTabBar::tab:selected { background:%5; color:#ffffff; }
        QPushButton { background:%3; color:%2; border:1px solid rgba(255,255,255,0.10); border-radius:%4px; padding:6px 10px; }
        QPushButton:hover { border-color:%5; }
        QMenu { background:%1; color:%2; border:1px solid rgba(255,255,255,0.10); }
        QHeaderView::section { background:%3; color:%2; padding:6px; border:0; }
    )")
        .arg(window.name(), text.name(), baseColor.name(), QString::number(radius), highlight.name());
}
}

void ThemeManager::apply(QApplication &app, const SettingsManager &settings) {
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    const InstalledTheme theme = activeTheme(settings);
    QPalette palette = app.palette();
    app.setStyleSheet(presetStyle(palette, settings, theme));
}

QVector<InstalledTheme> ThemeManager::installedThemes() {
    QVector<InstalledTheme> out;
    QDir dir(AppPaths::themesDir());
    const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &name : entries) {
        const InstalledTheme t = fromRoot(dir.absoluteFilePath(name));
        if (!t.id.isEmpty()) out.push_back(t);
    }
    return out;
}

InstalledTheme ThemeManager::activeTheme(const SettingsManager &settings) {
    const QString id = settings.stringValue(QStringLiteral("appearance/activeThemeId"));
    for (const auto &theme : installedThemes()) if (theme.id == id) return theme;
    return {};
}

QString ThemeManager::currentAnimatedAsset(const SettingsManager &settings) {
    return activeTheme(settings).animatedAsset;
}

bool ThemeManager::extractArchive(const QString &archivePath, const QString &targetDir) {
    QDir().mkpath(targetDir);
    if (QProcess::execute(QStringLiteral("bsdtar"), {QStringLiteral("-xf"), archivePath, QStringLiteral("-C"), targetDir}) == 0) return true;
    return QProcess::execute(QStringLiteral("unzip"), {QStringLiteral("-o"), archivePath, QStringLiteral("-d"), targetDir}) == 0;
}

bool ThemeManager::installThemePackage(const QString &path, const QString &source, QString *message) {
    Q_UNUSED(source);
    AppPaths::ensureAll();
    QString rootDir;
    if (QFileInfo(path).isDir()) {
        rootDir = path;
    } else {
        rootDir = AppPaths::tempDir() + QStringLiteral("/theme-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!extractArchive(path, rootDir)) {
            if (message) *message = QStringLiteral("Не удалось распаковать тему: %1").arg(path);
            return false;
        }
    }
    InstalledTheme theme = fromRoot(rootDir);
    if (theme.id.isEmpty()) {
        if (message) *message = QStringLiteral("В пакете нет manifest theme.");
        return false;
    }
    const QString installDir = AppPaths::themesDir() + QStringLiteral("/") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(installDir);
    if (QFileInfo(rootDir).isDir() && rootDir != installDir) {
        QDir src(rootDir);
        for (const auto &entry : src.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString srcPath = src.absoluteFilePath(entry);
            const QString dstPath = installDir + QStringLiteral("/") + entry;
            if (QFileInfo(srcPath).isDir()) {
                QProcess::execute(QStringLiteral("cp"), {QStringLiteral("-a"), srcPath, dstPath});
            } else {
                QFile::copy(srcPath, dstPath);
            }
        }
    }
    InstalledTheme installed = fromRoot(installDir);
    if (message) {
        *message = installed.animated
            ? QStringLiteral("Установлена тема %1 (с анимированным ассетом).")
                  .arg(installed.title)
            : QStringLiteral("Установлена тема %1.").arg(installed.title);
    }
    return true;
}
