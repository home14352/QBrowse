#include "AppPaths.h"
#include "BookmarkStore.h"
#include "DownloadManager.h"
#include "GoogleAccountService.h"
#include "HistoryStore.h"
#include "ImportManager.h"
#include "MainWindow.h"
#include "MozillaAccountService.h"
#include "SettingsManager.h"
#include "StoreInstaller.h"

#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QGuiApplication>
#include <QSettings>

int main(int argc, char **argv) {
    QCoreApplication::setOrganizationName(QStringLiteral("OpenAI"));
    QCoreApplication::setApplicationName(QStringLiteral("QBrowse"));
    QGuiApplication::setDesktopFileName(QStringLiteral("qbrowse.desktop"));
    AppPaths::ensureAll();

    QSettings preSettings(AppPaths::settingsFile(), QSettings::IniFormat);
    QString flags = preSettings.value(QStringLiteral("advanced/externalFlags")).toString();
    const QString dohMode = preSettings.value(QStringLiteral("privacy/dohMode"), QStringLiteral("off")).toString();
    const QString dohTemplates = preSettings.value(QStringLiteral("privacy/dohServers")).toString();
    if (dohMode != QStringLiteral("off")) {
        flags += QStringLiteral(" --dns-over-https-mode=%1").arg(dohMode);
        if (!dohTemplates.trimmed().isEmpty()) flags += QStringLiteral(" --dns-over-https-templates=\"") + dohTemplates + QStringLiteral("\"");
    }
    if (!flags.trimmed().isEmpty()) qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags.toUtf8());

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/logo/qbrowse-logo.svg")));

    SettingsManager settings;
    BookmarkStore bookmarks;
    HistoryStore history;
    DownloadManager downloads;
    ImportManager imports(&bookmarks, &history);
    MozillaAccountService mozilla(&settings);
    GoogleAccountService google(&settings);
    StoreInstaller storeInstaller(&settings);

    MainWindow window(&settings, &bookmarks, &history, &downloads, &imports, &mozilla, &google, &storeInstaller);
    window.show();

    QStringList urls;
    const auto args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (!arg.startsWith('-')) urls.push_back(arg);
    }
    if (!urls.isEmpty()) window.openUrls(urls);

    return app.exec();
}
