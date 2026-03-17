#pragma once
#include <QObject>
#include <QPointer>

class QWebEngineProfile;
class QWebEngineExtensionInfo;
class SettingsManager;

class StoreInstaller : public QObject {
    Q_OBJECT
public:
    explicit StoreInstaller(SettingsManager *settings, QObject *parent = nullptr);
    bool installFromChromeStore(const QString &idOrUrl, QWebEngineProfile *profile, QString *message = nullptr);
    bool installFromFirefoxStore(const QString &slugOrUrl, QWebEngineProfile *profile, QString *message = nullptr);
    bool installFromLocalArchive(const QString &path, QWebEngineProfile *profile, QString *message = nullptr);
    bool openExtensionPopup(const QWebEngineExtensionInfo &ext, QWebEngineProfile *profile, QWidget *parent = nullptr);
    bool installUserDesktopFile(const QString &binaryPath, QString *message = nullptr);
    bool makeDefaultBrowser(QString *message = nullptr);
private:
    SettingsManager *m_settings;
    QByteArray downloadSync(const QUrl &url, QString *error = nullptr);
    QString extractChromeId(const QString &input) const;
    QString extractFirefoxSlug(const QString &input) const;
    QString crxToZip(const QByteArray &crx, QString *error = nullptr) const;
    bool isThemeArchive(const QString &path) const;
};
