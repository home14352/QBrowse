#include "StoreInstaller.h"
#include "AppPaths.h"
#include "SettingsManager.h"
#include "ThemeManager.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineExtensionInfo>
#include <QWebEngineExtensionManager>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QFileInfo>
#include <QUuid>
#include <QDir>
#include <QNetworkRequest>
#include <QEventLoop>

StoreInstaller::StoreInstaller(SettingsManager *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {}

    QByteArray StoreInstaller::downloadSync(const QUrl &url, QString *message) {
        QNetworkAccessManager network;
        QNetworkRequest request(url);
        QNetworkReply *reply = network.get(request);

        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        const QByteArray data = reply->readAll();

        if (reply->error() != QNetworkReply::NoError && message) {
            *message = reply->errorString();
        }

        reply->deleteLater();
        return data;
    }

QString StoreInstaller::extractChromeId(const QString &input) const {
    QRegularExpression re(QStringLiteral("([a-p]{32})"));
    const auto match = re.match(input);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString StoreInstaller::extractFirefoxSlug(const QString &input) const {
    QRegularExpression re(QStringLiteral("/addon/([^/]+)/?"));
    const auto match = re.match(input);
    return match.hasMatch() ? match.captured(1) : input.trimmed();
}

QString StoreInstaller::crxToZip(const QByteArray &crx, QString *extractError) const {
    if (crx.size() < 16 || crx.left(4) != QByteArrayLiteral("Cr24")) {
        if (extractError) *extractError = QStringLiteral("Файл не похож на CRX.");
        return {};
    }
    auto le32 = [&](int offset) -> quint32 {
        return quint32(quint8(crx[offset])) | (quint32(quint8(crx[offset + 1])) << 8) | (quint32(quint8(crx[offset + 2])) << 16) | (quint32(quint8(crx[offset + 3])) << 24);
    };
    const quint32 version = le32(4);
    quint32 zipOffset = 0;
    if (version == 2) {
        const quint32 pubLen = le32(8);
        const quint32 sigLen = le32(12);
        zipOffset = 16 + pubLen + sigLen;
    } else if (version == 3) {
        const quint32 headerSize = le32(8);
        zipOffset = 12 + headerSize;
    } else {
        if (extractError) *extractError = QStringLiteral("Неподдерживаемая версия CRX.");
        return {};
    }
    if (zipOffset >= quint32(crx.size())) {
        if (extractError) *extractError = QStringLiteral("Пустая ZIP-часть CRX.");
        return {};
    }
    const QString zipPath = AppPaths::tempDir() + QStringLiteral("/") + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".zip");
    QFile f(zipPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (extractError) *extractError = QStringLiteral("Не удалось записать временный ZIP.");
        return {};
    }
    f.write(crx.mid(zipOffset));
    return zipPath;
}

bool StoreInstaller::isThemeArchive(const QString &path) const {
    const QString tempRoot = AppPaths::tempDir() + QStringLiteral("/inspect-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!ThemeManager::extractArchive(path, tempRoot)) return false;
    QFile manifest(tempRoot + QStringLiteral("/manifest.json"));
    if (!manifest.open(QIODevice::ReadOnly)) return false;
    const auto obj = QJsonDocument::fromJson(manifest.readAll()).object();
    return obj.contains(QStringLiteral("theme"));
}

bool StoreInstaller::installFromLocalArchive(const QString &path, QWebEngineProfile *profile, QString *message) {
    if (isThemeArchive(path)) return ThemeManager::installThemePackage(path, QStringLiteral("local"), message);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    if (profile && profile->extensionManager()) {
        profile->extensionManager()->installExtension(path);
        if (message) *message = QStringLiteral("Установка расширения запущена.");
        return true;
    }
#endif
    if (message) *message = QStringLiteral("Менеджер расширений Qt недоступен.");
    return false;
}

bool StoreInstaller::installFromChromeStore(const QString &idOrUrl, QWebEngineProfile *profile, QString *message) {
    const QString id = extractChromeId(idOrUrl);
    if (id.isEmpty()) { if (message) *message = QStringLiteral("Не удалось извлечь ID расширения Chrome Web Store."); return false; }
    const QString downloadUrl = QStringLiteral("https://clients2.google.com/service/update2/crx?response=redirect&prodversion=114.0&acceptformat=crx2,crx3&x=id%%3D%1%%26installsource%%3Dondemand%%26uc").arg(id);
    QString extractError;
    const QByteArray crx = downloadSync(QUrl(downloadUrl), &extractError);
    if (crx.isEmpty()) { if (message) *message = QStringLiteral("Не удалось скачать CRX: %1").arg(extractError); return false; }
    const QString zipPath = crxToZip(crx, &extractError);
    if (zipPath.isEmpty()) { if (message) *message = extractError; return false; }
    return installFromLocalArchive(zipPath, profile, message);
}

bool StoreInstaller::installFromFirefoxStore(const QString &slugOrUrl, QWebEngineProfile *profile, QString *message) {
    if (!m_settings->boolValue(QStringLiteral("privacy/allowFirefoxStoreInstall"), false)) {
        if (message) *message = QStringLiteral("Включи галочку 'Разрешить установку из Firefox Add-ons' в настройках.");
        return false;
    }
    const QString slug = extractFirefoxSlug(slugOrUrl);
    if (slug.isEmpty()) { if (message) *message = QStringLiteral("Не удалось определить slug дополнения Firefox."); return false; }
    QString extractError;
    const QByteArray meta = downloadSync(QUrl(QStringLiteral("https://addons.mozilla.org/api/v5/addons/addon/%1/").arg(slug)), &extractError);
    if (meta.isEmpty()) { if (message) *message = QStringLiteral("Не удалось получить данные AMO: %1").arg(extractError); return false; }
    const auto obj = QJsonDocument::fromJson(meta).object();
    const QString xpiUrl = obj.value(QStringLiteral("current_version")).toObject().value(QStringLiteral("file")).toObject().value(QStringLiteral("url")).toString();
    if (xpiUrl.isEmpty()) { if (message) *message = QStringLiteral("AMO не вернул URL XPI."); return false; }
    const QByteArray xpi = downloadSync(QUrl(xpiUrl), &extractError);
    if (xpi.isEmpty()) { if (message) *message = QStringLiteral("Не удалось скачать XPI: %1").arg(extractError); return false; }
    const QString xpiPath = AppPaths::tempDir() + QStringLiteral("/") + slug + QStringLiteral(".xpi");
    QFile file(xpiPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        extractError = QStringLiteral("Не удалось открыть файл XPI для записи: %1").arg(xpiPath);
        return false;
    }
    file.write(xpi);
    file.close();
    return installFromLocalArchive(xpiPath, profile, message);
}

bool StoreInstaller::openExtensionPopup(const QWebEngineExtensionInfo &ext, QWebEngineProfile *profile, QWidget *parent) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    if (!profile || !ext.actionPopupUrl().isValid()) return false;
    auto *dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(ext.name());
    dialog->resize(420, 620);
    auto *layout = new QVBoxLayout(dialog);
    auto *view = new QWebEngineView(dialog);
    auto *page = new QWebEnginePage(profile, view);
    view->setPage(page);
    view->load(ext.actionPopupUrl());
    layout->addWidget(view);
    dialog->show();
    return true;
#else
    Q_UNUSED(ext); Q_UNUSED(profile); Q_UNUSED(parent); return false;
#endif
}

bool StoreInstaller::installUserDesktopFile(const QString &binaryPath, QString *message) {
    const QString appDir = QDir::homePath() + QStringLiteral("/.local/share/applications");
    const QString iconDir = QDir::homePath() + QStringLiteral("/.local/share/icons/hicolor/scalable/apps");
    QDir().mkpath(appDir); QDir().mkpath(iconDir);
    QFile desktop(appDir + QStringLiteral("/qbrowse.desktop"));
    if (!desktop.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (message) *message = QStringLiteral("Не удалось создать desktop-файл.");
        return false;
    }
    QTextStream out(&desktop);
    out << "[Desktop Entry]\nType=Application\nName=QBrowse\nGenericName=Web Browser\nExec=" << binaryPath << " %u\nIcon=qbrowse\nTerminal=false\nCategories=Network;WebBrowser;\nMimeType=text/html;text/xml;application/xhtml+xml;x-scheme-handler/http;x-scheme-handler/https;\nStartupNotify=true\n";
    QFile::remove(iconDir + QStringLiteral("/qbrowse.svg"));
    QFile::copy(QStringLiteral(":/logo/qbrowse-logo.svg"), iconDir + QStringLiteral("/qbrowse.svg"));
    QProcess::execute(QStringLiteral("update-desktop-database"), {appDir});
    if (message) *message = QStringLiteral("Ярлык установлен в ~/.local/share/applications.");
    return true;
}

bool StoreInstaller::makeDefaultBrowser(QString *message) {
    const QString id = m_settings->stringValue(QStringLiteral("system/desktopFileId"), QStringLiteral("qbrowse.desktop"));
    const int a = QProcess::execute(QStringLiteral("xdg-settings"), {QStringLiteral("set"), QStringLiteral("default-web-browser"), id});
    const int b = QProcess::execute(QStringLiteral("xdg-mime"), {QStringLiteral("default"), id, QStringLiteral("x-scheme-handler/http"), QStringLiteral("x-scheme-handler/https"), QStringLiteral("text/html"), QStringLiteral("application/xhtml+xml")});
    if (message) *message = (a == 0 && b == 0)
        ? QStringLiteral("QBrowse назначен браузером по умолчанию.")
        : QStringLiteral("Команда выполнена частично. Проверь desktop file и xdg-utils.");
    return a == 0 && b == 0;
}
