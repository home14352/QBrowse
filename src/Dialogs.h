#pragma once
#include <QDialog>

#include "ImportManager.h"

class BookmarkStore;
class HistoryStore;
class DownloadManager;
class SettingsManager;
class MozillaAccountService;
class GoogleAccountService;
class StoreInstaller;
class QLineEdit;
class QTableWidget;
class QTextEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QTabWidget;
class QWebEngineProfile;
class QWebEngineExtensionManager;

class BookmarksDialog : public QDialog {
    Q_OBJECT
public:
    explicit BookmarksDialog(BookmarkStore *store, QWidget *parent = nullptr);
signals:
    void openUrlRequested(const QString &url);
private:
    void refresh();
    BookmarkStore *m_store;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
};

class HistoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit HistoryDialog(HistoryStore *store, QWidget *parent = nullptr);
signals:
    void openUrlRequested(const QString &url);
private:
    void refresh();
    HistoryStore *m_store;
    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_table = nullptr;
};

class DownloadsDialog : public QDialog {
    Q_OBJECT
public:
    explicit DownloadsDialog(DownloadManager *manager, QWidget *parent = nullptr);
private:
    void refresh();
    DownloadManager *m_manager;
    QTableWidget *m_table = nullptr;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(SettingsManager *settings,
                   ImportManager *importManager,
                   MozillaAccountService *mozilla,
                   GoogleAccountService *google,
                   QWebEngineProfile *profile,
                   StoreInstaller *storeInstaller,
                   QWidget *parent = nullptr);
signals:
    void settingsApplied();
private:
    void buildUi();
    void loadFromSettings();
    void saveToSettings();
    void refreshAccounts();
    void refreshExtensions();
    void refreshPermissions();
    void refreshThemes();
    void appendImportReport(const ImportReport &report);

    SettingsManager *m_settings;
    ImportManager *m_importManager;
    MozillaAccountService *m_mozilla;
    GoogleAccountService *m_google;
    QWebEngineProfile *m_profile;
    StoreInstaller *m_storeInstaller;
    QWebEngineExtensionManager *m_extensionManager = nullptr;

    QTabWidget *m_tabs = nullptr;
    QTextEdit *m_log = nullptr;

    QComboBox *m_languageCombo = nullptr;
    QLineEdit *m_homepageEdit = nullptr;
    QComboBox *m_searchEngineCombo = nullptr;
    QLineEdit *m_downloadPathEdit = nullptr;
    QCheckBox *m_restoreSessionCheck = nullptr;
    QCheckBox *m_showBookmarksBarCheck = nullptr;

    QComboBox *m_presetCombo = nullptr;
    QCheckBox *m_verticalTabsCheck = nullptr;
    QCheckBox *m_iconOnlyVerticalTabsCheck = nullptr;
    QCheckBox *m_compactModeCheck = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QTableWidget *m_themeTable = nullptr;
    QLineEdit *m_themeChromeStoreEdit = nullptr;
    QLineEdit *m_themeFirefoxStoreEdit = nullptr;

    QTableWidget *m_extensionsTable = nullptr;
    QLineEdit *m_chromeStoreEdit = nullptr;
    QLineEdit *m_firefoxStoreEdit = nullptr;

    QCheckBox *m_doNotTrackCheck = nullptr;
    QCheckBox *m_blockThirdPartyCookiesCheck = nullptr;
    QCheckBox *m_javascriptCheck = nullptr;
    QCheckBox *m_enableHostsBlocklistCheck = nullptr;
    QLineEdit *m_hostsPathEdit = nullptr;
    QCheckBox *m_allowFirefoxStoreInstallCheck = nullptr;
    QComboBox *m_dohModeCombo = nullptr;
    QLineEdit *m_dohTemplatesEdit = nullptr;

    QLineEdit *m_mozillaClientId = nullptr;
    QLineEdit *m_mozillaClientSecret = nullptr;
    QLineEdit *m_mozillaStatus = nullptr;
    QLineEdit *m_mozillaEmail = nullptr;
    QLineEdit *m_mozillaError = nullptr;

    QLineEdit *m_googleClientId = nullptr;
    QLineEdit *m_googleClientSecret = nullptr;
    QLineEdit *m_googleStatus = nullptr;
    QLineEdit *m_googleEmail = nullptr;
    QLineEdit *m_googleError = nullptr;

    QTableWidget *m_permissionsTable = nullptr;
    QLineEdit *m_userAgentSuffixEdit = nullptr;
    QLineEdit *m_externalFlagsEdit = nullptr;
};
