#pragma once
#include <QMainWindow>

class SettingsManager;
class BookmarkStore;
class HistoryStore;
class DownloadManager;
class ImportManager;
class MozillaAccountService;
class GoogleAccountService;
class StoreInstaller;
class RequestInterceptor;
class BrowserView;
class BookmarksDialog;
class HistoryDialog;
class DownloadsDialog;
class SettingsDialog;
class QDockWidget;
class QListWidget;
class QLineEdit;
class QTabWidget;
class QToolBar;
class QAction;
class QWebEngineProfile;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(SettingsManager *settings,
               BookmarkStore *bookmarks,
               HistoryStore *history,
               DownloadManager *downloads,
               ImportManager *importManager,
               MozillaAccountService *mozilla,
               GoogleAccountService *google,
               StoreInstaller *storeInstaller,
               bool privateMode = false,
               QWidget *parent = nullptr);
    BrowserView *createDetachedView();
    BrowserView *createTab(const QUrl &url, bool foreground = true, bool pinned = false);
    void openUrls(const QStringList &urls);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    BrowserView *currentView() const;
    void buildUi();
    void buildToolBars();
    void buildMenus();
    void buildSidebar();
    void navigate(BrowserView *view, const QUrl &url);
    void showHomePage(BrowserView *view);
    void showAboutPage(BrowserView *view);
    void hookTab(BrowserView *view);
    QUrl userInputToUrl(const QString &text) const;
    QString displayUrlForView(BrowserView *view) const;
    void restoreSessionOrHomepage();
    void saveSession() const;
    void applySettings();
    void updateWindowTitle();
    void updateTabVisual(int index);
    void rebuildSidebar();
    void rebuildBookmarkBar();
    void ensureSettingsDialog();
    void openPrivateWindow();
    void moveTabToNewWindow(int index);
    void closeTab(int index);
    void reopenClosedTab();
    void duplicateCurrentTab();
    void toggleMuteCurrentTab();
    void togglePinCurrentTab();
    void showCommandPalette();

    SettingsManager *m_settings;
    BookmarkStore *m_bookmarks;
    HistoryStore *m_history;
    DownloadManager *m_downloads;
    ImportManager *m_importManager;
    MozillaAccountService *m_mozilla;
    GoogleAccountService *m_google;
    StoreInstaller *m_storeInstaller;
    bool m_privateMode;
    QWebEngineProfile *m_profile;
    RequestInterceptor *m_interceptor;
    QString m_baseUserAgent;

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QDockWidget *m_sidebarDock = nullptr;
    QListWidget *m_sidebarBookmarks = nullptr;
    QListWidget *m_sidebarHistory = nullptr;
    QToolBar *m_bookmarkBar = nullptr;
    QAction *m_reopenClosedAction = nullptr;
    QAction *m_toggleSidebarAction = nullptr;

    BookmarksDialog *m_bookmarksDialog = nullptr;
    HistoryDialog *m_historyDialog = nullptr;
    DownloadsDialog *m_downloadsDialog = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;

    QStringList m_closedTabs;
};
