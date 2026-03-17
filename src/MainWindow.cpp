#include "MainWindow.h"
#include "AppPaths.h"
#include "BookmarkStore.h"
#include "BrowserView.h"
#include "Dialogs.h"
#include "DownloadManager.h"
#include "GoogleAccountService.h"
#include "HistoryStore.h"
#include "ImportManager.h"
#include "MozillaAccountService.h"
#include "RequestInterceptor.h"
#include "SettingsManager.h"
#include "StoreInstaller.h"
#include "ThemeManager.h"
#include <QUrl>
#include <QIcon>
#include <QApplication>
#include <QAction>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QDialog>
#include <QDockWidget>
#include <QFile>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWebEngineCookieStore>
#include <QWebEngineDownloadRequest>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace {

    class SmartTabBar : public QTabBar {
    public:
        explicit SmartTabBar(QWidget *parent = nullptr)
        : QTabBar(parent)
        {
            setMovable(true);
            setUsesScrollButtons(true);
            setExpanding(false);
        }

    protected:
        QSize tabSizeHint(int index) const override {
            QSize size = QTabBar::tabSizeHint(index);
            const bool vertical =
            shape() == QTabBar::RoundedWest ||
            shape() == QTabBar::TriangularWest;

            if (vertical && property("iconOnly").toBool()) {
                size.setWidth(44);
                size.setHeight(qMax(size.height(), 40));
            }
            return size;
        }
    };

    class ExposedTabWidget : public QTabWidget {
    public:
        using QTabWidget::setTabBar;
        using QTabWidget::tabBar;

        explicit ExposedTabWidget(QWidget *parent = nullptr)
        : QTabWidget(parent)
        {
        }
    };

} // namespace

QString specialTitle(const QString &special) {
    if (special == QStringLiteral("qbrowse://home")) return QStringLiteral("Домашняя страница");
    if (special == QStringLiteral("qbrowse://about")) return QStringLiteral("О QBrowse");
    return QStringLiteral("Новая вкладка");
}

MainWindow::MainWindow(SettingsManager *settings,
                       BookmarkStore *bookmarks,
                       HistoryStore *history,
                       DownloadManager *downloads,
                       ImportManager *importManager,
                       MozillaAccountService *mozilla,
                       GoogleAccountService *google,
                       StoreInstaller *storeInstaller,
                       bool privateMode,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_bookmarks(bookmarks)
    , m_history(history)
    , m_downloads(downloads)
    , m_importManager(importManager)
    , m_mozilla(mozilla)
    , m_google(google)
    , m_storeInstaller(storeInstaller)
    , m_privateMode(privateMode) {
    setWindowIcon(QIcon(QStringLiteral(":/logo/qbrowse-logo.svg")));
    resize(1450, 900);

    if (m_privateMode) {
        m_profile = new QWebEngineProfile(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        m_profile->setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory);
#endif
    } else {
        m_profile = new QWebEngineProfile(QStringLiteral("QBrowseProfile"), this);
        m_profile->setPersistentStoragePath(AppPaths::profileDir());
        m_profile->setCachePath(AppPaths::cacheDir());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        m_profile->setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
#endif
    }
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::PersistentCookiesPolicy::ForcePersistentCookies);
    m_interceptor = new RequestInterceptor(m_settings, this);
    m_profile->setUrlRequestInterceptor(m_interceptor);
    m_baseUserAgent = m_profile->httpUserAgent();

    buildUi();
    buildToolBars();
    buildMenus();
    buildSidebar();

    m_bookmarksDialog = new BookmarksDialog(m_bookmarks, this);
    m_historyDialog = new HistoryDialog(m_history, this);
    m_downloadsDialog = new DownloadsDialog(m_downloads, this);
    connect(m_bookmarksDialog, &BookmarksDialog::openUrlRequested, this, [this](const QString &u) { navigate(currentView(), userInputToUrl(u)); });
    connect(m_historyDialog, &HistoryDialog::openUrlRequested, this, [this](const QString &u) { navigate(currentView(), userInputToUrl(u)); });
    connect(m_bookmarks, &BookmarkStore::changed, this, [this]() { rebuildBookmarkBar(); rebuildSidebar(); });
    connect(m_history, &HistoryStore::changed, this, [this]() { rebuildSidebar(); });
    connect(m_downloads, &DownloadManager::changed, this, [this]() { if (m_downloadsDialog->isVisible()) m_downloadsDialog->update(); });
    connect(m_profile, &QWebEngineProfile::downloadRequested, this, [this](QWebEngineDownloadRequest *req) { m_downloads->handleDownload(req); });

    restoreSessionOrHomepage();
    applySettings();
}

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *tabs = new ExposedTabWidget(central);
    tabs->setDocumentMode(true);
    tabs->setMovable(true);
    tabs->setTabsClosable(true);
    tabs->setTabBar(new SmartTabBar(tabs));
    m_tabs = tabs;
    layout->addWidget(m_tabs, 1);
    setCentralWidget(central);

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        if (auto *view = qobject_cast<BrowserView *>(m_tabs->widget(idx))) m_urlEdit->setText(displayUrlForView(view));
        updateWindowTitle();
    });
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabs->tabBar(), &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const int index = m_tabs->tabBar()->tabAt(pos);
        if (index < 0) return;
        QMenu menu(this);
        menu.addAction(QStringLiteral("Дублировать вкладку"), this, &MainWindow::duplicateCurrentTab);
        menu.addAction(QStringLiteral("Вынести в новое окно"), this, [this, index]() { moveTabToNewWindow(index); });
        menu.addAction(QStringLiteral("Закрепить/открепить"), this, &MainWindow::togglePinCurrentTab);
        menu.addAction(QStringLiteral("Закрыть"), this, [this, index]() { closeTab(index); });
        menu.exec(m_tabs->tabBar()->mapToGlobal(pos));
    });

    statusBar()->showMessage(QStringLiteral("Готово"));
}

void MainWindow::buildToolBars() {
    auto *nav = addToolBar(QStringLiteral("Навигация"));
    nav->setMovable(false);
    nav->setIconSize(QSize(18, 18));

    auto *back = nav->addAction(QStringLiteral("←"));
    auto *forward = nav->addAction(QStringLiteral("→"));
    auto *reload = nav->addAction(QStringLiteral("⟳"));
    auto *home = nav->addAction(QStringLiteral("⌂"));
    nav->addSeparator();

    auto *logoLabel = new QLabel;
    logoLabel->setPixmap(QIcon(QStringLiteral(":/logo/qbrowse-logo.svg")).pixmap(18, 18));
    nav->addWidget(logoLabel);

    m_urlEdit = new QLineEdit;
    m_urlEdit->setPlaceholderText(QStringLiteral("Адрес или поисковый запрос"));
    connect(m_urlEdit, &QLineEdit::returnPressed, this, [this]() { navigate(currentView(), userInputToUrl(m_urlEdit->text())); });
    nav->addWidget(m_urlEdit);

    auto *newTab = nav->addAction(QStringLiteral("+") );
    auto *star = nav->addAction(QStringLiteral("★"));
    auto *settings = nav->addAction(QStringLiteral("⚙"));

    connect(back, &QAction::triggered, this, [this]() { if (auto *v = currentView()) v->back(); });
    connect(forward, &QAction::triggered, this, [this]() { if (auto *v = currentView()) v->forward(); });
    connect(reload, &QAction::triggered, this, [this]() { if (auto *v = currentView()) v->reload(); });
    connect(home, &QAction::triggered, this, [this]() { navigate(currentView(), userInputToUrl(m_settings->stringValue(QStringLiteral("general/homepage"), QStringLiteral("qbrowse://home")))); });
    connect(newTab, &QAction::triggered, this, [this]() { createTab(userInputToUrl(m_settings->stringValue(QStringLiteral("general/homepage"))), true); });
    connect(star, &QAction::triggered, this, [this]() {
        if (auto *v = currentView()) {
            m_bookmarks->addBookmark(v->title(), v->url().toString(), QStringLiteral("Панель"));
            statusBar()->showMessage(QStringLiteral("Страница добавлена в закладки"), 2000);
        }
    });
    connect(settings, &QAction::triggered, this, [this]() { ensureSettingsDialog(); m_settingsDialog->show(); m_settingsDialog->raise(); });

    m_bookmarkBar = addToolBar(QStringLiteral("Закладки"));
    m_bookmarkBar->setMovable(false);
    rebuildBookmarkBar();
}

void MainWindow::buildMenus() {
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("Файл"));
    fileMenu->addAction(QStringLiteral("Новая вкладка"), this, [this]() { createTab(userInputToUrl(m_settings->stringValue(QStringLiteral("general/homepage"))), true); }, QKeySequence(QStringLiteral("Ctrl+T")));
    fileMenu->addAction(QStringLiteral("Новое окно"), this, [this]() { auto *w = new MainWindow(m_settings, m_bookmarks, m_history, m_downloads, m_importManager, m_mozilla, m_google, m_storeInstaller); w->setAttribute(Qt::WA_DeleteOnClose); w->show(); }, QKeySequence::New);
    fileMenu->addAction(QStringLiteral("Новое приватное окно"), this, &MainWindow::openPrivateWindow, QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    fileMenu->addAction(QStringLiteral("Вынести текущую вкладку"), this, [this]() { moveTabToNewWindow(m_tabs->currentIndex()); });
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Выход"), this, &QWidget::close, QKeySequence::Quit);

    auto *editMenu = menuBar()->addMenu(QStringLiteral("Правка"));
    editMenu->addAction(QStringLiteral("Фокус на адресной строке"), this, [this]() { m_urlEdit->setFocus(); m_urlEdit->selectAll(); }, QKeySequence(QStringLiteral("Ctrl+L")));
    editMenu->addAction(QStringLiteral("Командная палитра"), this, &MainWindow::showCommandPalette, QKeySequence(QStringLiteral("Ctrl+K")));
    editMenu->addAction(QStringLiteral("Повторно открыть закрытую вкладку"), this, &MainWindow::reopenClosedTab, QKeySequence(QStringLiteral("Ctrl+Shift+T")));

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("Вид"));
    m_toggleSidebarAction = viewMenu->addAction(QStringLiteral("Боковая панель"));
    m_toggleSidebarAction->setCheckable(true);
    connect(m_toggleSidebarAction, &QAction::toggled, this, [this](bool on) { if (m_sidebarDock) m_sidebarDock->setVisible(on); });
    viewMenu->addAction(QStringLiteral("Закладки"), this, [this]() { m_bookmarksDialog->show(); m_bookmarksDialog->raise(); }, QKeySequence(QStringLiteral("Ctrl+Shift+B")));
    viewMenu->addAction(QStringLiteral("История"), this, [this]() { m_historyDialog->show(); m_historyDialog->raise(); }, QKeySequence(QStringLiteral("Ctrl+H")));
    viewMenu->addAction(QStringLiteral("Загрузки"), this, [this]() { m_downloadsDialog->show(); m_downloadsDialog->raise(); }, QKeySequence(QStringLiteral("Ctrl+J")));

    auto *toolsMenu = menuBar()->addMenu(QStringLiteral("Инструменты"));
    toolsMenu->addAction(QStringLiteral("Настройки"), this, [this]() { ensureSettingsDialog(); m_settingsDialog->show(); m_settingsDialog->raise(); }, QKeySequence::Preferences);
    toolsMenu->addAction(QStringLiteral("Сделать браузером по умолчанию"), this, [this]() {
        QString msg; m_storeInstaller->makeDefaultBrowser(&msg); QMessageBox::information(this, QStringLiteral("QBrowse"), msg);
    });
    toolsMenu->addAction(QStringLiteral("Установить ярлык в систему"), this, [this]() {
        QString msg; m_storeInstaller->installUserDesktopFile(QCoreApplication::applicationFilePath(), &msg); QMessageBox::information(this, QStringLiteral("QBrowse"), msg);
    });
    toolsMenu->addAction(QStringLiteral("Дублировать вкладку"), this, &MainWindow::duplicateCurrentTab);
    toolsMenu->addAction(QStringLiteral("Выключить/включить звук вкладки"), this, &MainWindow::toggleMuteCurrentTab);
    toolsMenu->addAction(QStringLiteral("Закрепить/открепить вкладку"), this, &MainWindow::togglePinCurrentTab);

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("Справка"));
    helpMenu->addAction(QStringLiteral("Домашняя страница"), this, [this]() { navigate(currentView(), QUrl(QStringLiteral("qbrowse://home"))); });
    helpMenu->addAction(QStringLiteral("О QBrowse"), this, [this]() { navigate(currentView(), QUrl(QStringLiteral("qbrowse://about"))); }, QKeySequence(QStringLiteral("F1")));
}

void MainWindow::buildSidebar() {
    m_sidebarDock = new QDockWidget(QStringLiteral("Быстрый доступ"), this);
    auto *tabs = new QTabWidget(m_sidebarDock);
    m_sidebarBookmarks = new QListWidget(tabs);
    m_sidebarHistory = new QListWidget(tabs);
    tabs->addTab(m_sidebarBookmarks, QStringLiteral("Закладки"));
    tabs->addTab(m_sidebarHistory, QStringLiteral("История"));
    m_sidebarDock->setWidget(tabs);
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);
    connect(m_sidebarBookmarks, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) { navigate(currentView(), userInputToUrl(item->data(Qt::UserRole).toString())); });
    connect(m_sidebarHistory, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) { navigate(currentView(), userInputToUrl(item->data(Qt::UserRole).toString())); });
    rebuildSidebar();
}

BrowserView *MainWindow::createTab(const QUrl &url, bool foreground, bool pinned) {
    auto *view = new BrowserView(m_profile, this, this);
    view->setProperty("pinned", pinned);
    hookTab(view);
    int index = m_tabs->addTab(view, QStringLiteral("Новая вкладка"));
    if (pinned) {
        int insertPos = 0;
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (m_tabs->widget(i)->property("pinned").toBool()) insertPos = i + 1;
        }
        m_tabs->tabBar()->moveTab(index, insertPos);
        index = insertPos;
    }
    if (foreground) m_tabs->setCurrentIndex(index);
    navigate(view, url);
    return view;
}

BrowserView *MainWindow::createDetachedView() {
    auto *w = new MainWindow(m_settings, m_bookmarks, m_history, m_downloads, m_importManager, m_mozilla, m_google, m_storeInstaller, m_privateMode);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
    return w->currentView();
}

void MainWindow::openUrls(const QStringList &urls) {
    if (urls.isEmpty()) return;
    if (m_tabs->count() == 1) closeTab(0);
    for (int i = 0; i < urls.size(); ++i) createTab(userInputToUrl(urls.at(i)), i == 0);
}

BrowserView *MainWindow::currentView() const {
    return qobject_cast<BrowserView *>(m_tabs->currentWidget());
}

void MainWindow::navigate(BrowserView *view, const QUrl &url) {
    if (!view) return;
    if (url.toString() == QStringLiteral("qbrowse://home")) { showHomePage(view); return; }
    if (url.toString() == QStringLiteral("qbrowse://about")) { showAboutPage(view); return; }
    view->setProperty("qbrowseSpecialAddress", QString());
    view->setUrl(url.isValid() ? url : QUrl(QStringLiteral("about:blank")));
}

void MainWindow::showHomePage(BrowserView *view) {
    const QString bg = ThemeManager::currentAnimatedAsset(*m_settings);
    QString html;
    QTextStream out(&html);
    out << "<!doctype html><html><head><meta charset='utf-8'><style>"
        << "body{font-family:system-ui,sans-serif;background:#0f172a;color:#fff;margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;}"
        << ".card{width:min(920px,92vw);background:rgba(15,23,42,.72);backdrop-filter:blur(12px);border:1px solid rgba(255,255,255,.10);border-radius:28px;padding:28px;}"
        << "a{color:#8bb8ff;text-decoration:none;margin-right:12px;}input{width:100%;padding:14px;border-radius:14px;border:0;background:rgba(255,255,255,.10);color:#fff;}";
    if (!bg.isEmpty()) out << "body{background:url('file://" << bg << "') center/cover no-repeat fixed,#0f172a;}";
    out << "</style></head><body><div class='card'><h1>QBrowse</h1><p>Красивый нативный Chromium-браузер для Linux.</p><p><a href='https://www.google.com'>Google</a><a href='https://duckduckgo.com'>DuckDuckGo</a><a href='https://addons.mozilla.org'>Firefox Add-ons</a><a href='https://chromewebstore.google.com'>Chrome Web Store</a></p><form action='https://duckduckgo.com/' method='get'><input name='q' placeholder='Быстрый поиск'/></form></div></body></html>";
    view->setProperty("qbrowseSpecialAddress", QStringLiteral("qbrowse://home"));
    view->setHtml(html, QUrl(QStringLiteral("https://qbrowse.local/home")));
    updateTabVisual(m_tabs->indexOf(view));
}

void MainWindow::showAboutPage(BrowserView *view) {
    QString html;
    QTextStream out(&html);
    out << "<!doctype html><html><head><meta charset='utf-8'><style>body{font-family:system-ui,sans-serif;background:#111827;color:#fff;margin:0;padding:24px;}table{width:100%;border-collapse:collapse;}td{padding:10px;border-bottom:1px solid rgba(255,255,255,.08);}code{background:rgba(255,255,255,.1);padding:2px 8px;border-radius:999px;}</style></head><body>";
    out << "<h1>QBrowse</h1><table>";
    out << "<tr><td>Профиль</td><td><code>" << AppPaths::profileDir().toHtmlEscaped() << "</code></td></tr>";
    out << "<tr><td>Закладки</td><td>" << m_bookmarks->entries().size() << "</td></tr>";
    out << "<tr><td>История</td><td>" << m_history->totalEntries() << "</td></tr>";
    out << "<tr><td>Mozilla Account</td><td>" << m_mozilla->status().toHtmlEscaped() << "</td></tr>";
    out << "<tr><td>Google Account</td><td>" << m_google->status().toHtmlEscaped() << "</td></tr>";
    out << "<tr><td>DoH</td><td>" << m_settings->stringValue(QStringLiteral("privacy/dohMode")).toHtmlEscaped() << "</td></tr>";
    out << "<tr><td>Темы</td><td>" << m_settings->stringValue(QStringLiteral("appearance/activeThemeId")).toHtmlEscaped() << "</td></tr>";
    out << "</table></body></html>";
    view->setProperty("qbrowseSpecialAddress", QStringLiteral("qbrowse://about"));
    view->setHtml(html, QUrl(QStringLiteral("https://qbrowse.local/about")));
    updateTabVisual(m_tabs->indexOf(view));
}

void MainWindow::hookTab(BrowserView *view) {
    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString &) { updateTabVisual(m_tabs->indexOf(view)); updateWindowTitle(); });
    connect(view, &QWebEngineView::iconChanged, this, [this, view](const QIcon &icon) { const int i = m_tabs->indexOf(view); if (i >= 0) { m_tabs->setTabIcon(i, icon); updateTabVisual(i); } });
    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &) { if (view == currentView()) m_urlEdit->setText(displayUrlForView(view)); updateTabVisual(m_tabs->indexOf(view)); });
    connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
        if (ok && view->property("qbrowseSpecialAddress").toString().isEmpty()) m_history->addVisit(view->title(), view->url().toString());
        updateTabVisual(m_tabs->indexOf(view));
        updateWindowTitle();
    });
}

QUrl MainWindow::userInputToUrl(const QString &text) const {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return QUrl(QStringLiteral("qbrowse://home"));
    if (trimmed.compare(QStringLiteral("qbrowse://home"), Qt::CaseInsensitive) == 0) return QUrl(QStringLiteral("qbrowse://home"));
    if (trimmed.compare(QStringLiteral("qbrowse://about"), Qt::CaseInsensitive) == 0 || trimmed.compare(QStringLiteral("about:qbrowse"), Qt::CaseInsensitive) == 0) return QUrl(QStringLiteral("qbrowse://about"));
    const bool looksLikeUrl = trimmed.contains(QStringLiteral("://")) || trimmed.contains('.') || trimmed.startsWith(QStringLiteral("localhost")) || trimmed.startsWith(QStringLiteral("file:/"));
    if (looksLikeUrl) return QUrl::fromUserInput(trimmed);
    QString prefix = QStringLiteral("https://duckduckgo.com/?q=");
    const QString engine = m_settings->stringValue(QStringLiteral("general/searchEngine"), QStringLiteral("DuckDuckGo"));
    if (engine == QStringLiteral("Google")) prefix = QStringLiteral("https://www.google.com/search?q=");
    return QUrl(prefix + QString::fromUtf8(QUrl::toPercentEncoding(trimmed)));
}

QString MainWindow::displayUrlForView(BrowserView *view) const {
    if (!view) return {};
    const QString special = view->property("qbrowseSpecialAddress").toString();
    return special.isEmpty() ? view->url().toString() : special;
}

void MainWindow::restoreSessionOrHomepage() {
    if (!m_privateMode && m_settings->boolValue(QStringLiteral("general/restoreSession"), true)) {
        QFile f(AppPaths::sessionFile());
        if (f.open(QIODevice::ReadOnly)) {
            const auto arr = QJsonDocument::fromJson(f.readAll()).array();
            if (!arr.isEmpty()) {
                for (int i = 0; i < arr.size(); ++i) createTab(userInputToUrl(arr.at(i).toString()), i == 0);
                return;
            }
        }
    }
    createTab(userInputToUrl(m_settings->stringValue(QStringLiteral("general/homepage"), QStringLiteral("qbrowse://home"))), true);
}

void MainWindow::saveSession() const {
    if (m_privateMode) return;
    QJsonArray arr;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *view = qobject_cast<BrowserView *>(m_tabs->widget(i))) arr.push_back(displayUrlForView(view));
    }
    QFile f(AppPaths::sessionFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void MainWindow::applySettings() {
    ThemeManager::apply(*qApp, *m_settings);
    m_interceptor->reload();
    const QString suffix = m_settings->stringValue(QStringLiteral("advanced/userAgentSuffix")).trimmed();
    m_profile->setHttpUserAgent(suffix.isEmpty() ? m_baseUserAgent : m_baseUserAgent + QStringLiteral(" ") + suffix);
    m_profile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, m_settings->boolValue(QStringLiteral("privacy/javascriptEnabled"), true));
    m_profile->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, true);
    m_profile->settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);

    const bool vertical = m_settings->boolValue(QStringLiteral("appearance/verticalTabs"), true);
    const bool iconOnly = m_settings->boolValue(QStringLiteral("appearance/iconOnlyVerticalTabs"), true);
    m_tabs->setTabPosition(vertical ? QTabWidget::West : QTabWidget::North);
    if (auto *bar = dynamic_cast<SmartTabBar *>(m_tabs->tabBar())) {
        bar->setProperty("iconOnly", iconOnly);
        bar->updateGeometry();
    }
    m_bookmarkBar->setVisible(m_settings->boolValue(QStringLiteral("bookmarks/showBar"), true));
    for (int i = 0; i < m_tabs->count(); ++i) updateTabVisual(i);
    rebuildBookmarkBar();
    rebuildSidebar();
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    QString title = m_privateMode ? QStringLiteral("QBrowse — приватное окно") : QStringLiteral("QBrowse");
    if (auto *view = currentView()) {
        const QString t = view->title().trimmed();
        if (!t.isEmpty()) title = t + (m_privateMode ? QStringLiteral(" — QBrowse Private") : QStringLiteral(" — QBrowse"));
    }
    setWindowTitle(title);
}

void MainWindow::updateTabVisual(int index) {
    if (index < 0 || index >= m_tabs->count()) return;
    auto *view = qobject_cast<BrowserView *>(m_tabs->widget(index));
    if (!view) return;
    const bool vertical = m_settings->boolValue(QStringLiteral("appearance/verticalTabs"), true);
    const bool iconOnly = m_settings->boolValue(QStringLiteral("appearance/iconOnlyVerticalTabs"), true);
    const QString special = view->property("qbrowseSpecialAddress").toString();
    const QString title = !special.isEmpty() ? specialTitle(special) : (view->title().isEmpty() ? QStringLiteral("Новая вкладка") : view->title());
    const QString url = displayUrlForView(view);
    const bool pinned = view->property("pinned").toBool();
    m_tabs->setTabToolTip(index, title + QStringLiteral("\n") + url);
    if (vertical && iconOnly) {
        m_tabs->setTabText(index, pinned ? QStringLiteral("•") : QStringLiteral(" "));
    } else {
        m_tabs->setTabText(index, pinned ? QStringLiteral("📌 ") + title : title);
    }
}

void MainWindow::rebuildSidebar() {
    m_sidebarBookmarks->clear();
    for (const auto &e : m_bookmarks->entries()) {
        auto *item = new QListWidgetItem(e.title.isEmpty() ? e.url : e.title);
        item->setToolTip(e.url);
        item->setData(Qt::UserRole, e.url);
        m_sidebarBookmarks->addItem(item);
    }
    m_sidebarHistory->clear();
    for (const auto &e : m_history->recent(150)) {
        auto *item = new QListWidgetItem(e.title.isEmpty() ? e.url : e.title);
        item->setToolTip(e.url + QStringLiteral("\n") + e.visitedAt.toLocalTime().toString(Qt::ISODate));
        item->setData(Qt::UserRole, e.url);
        m_sidebarHistory->addItem(item);
    }
}

void MainWindow::rebuildBookmarkBar() {
    const auto actions = m_bookmarkBar->actions();
    for (auto *a : actions) m_bookmarkBar->removeAction(a);
    for (const auto &e : m_bookmarks->entries()) {
        if (e.folder != QStringLiteral("Панель") && !e.pinned) continue;
        auto *act = m_bookmarkBar->addAction(e.title.isEmpty() ? e.url : e.title);
        connect(act, &QAction::triggered, this, [this, e]() { navigate(currentView(), userInputToUrl(e.url)); });
    }
}

void MainWindow::ensureSettingsDialog() {
    if (m_settingsDialog) return;
    m_settingsDialog = new SettingsDialog(m_settings, m_importManager, m_mozilla, m_google, m_profile, m_storeInstaller, this);
    connect(m_settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::applySettings);
}

void MainWindow::openPrivateWindow() {
    auto *w = new MainWindow(m_settings, m_bookmarks, m_history, m_downloads, m_importManager, m_mozilla, m_google, m_storeInstaller, true);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
}

void MainWindow::moveTabToNewWindow(int index) {
    if (index < 0 || index >= m_tabs->count()) return;
    auto *view = qobject_cast<BrowserView *>(m_tabs->widget(index));
    if (!view) return;
    const QString url = displayUrlForView(view);
    const bool pinned = view->property("pinned").toBool();
    auto *w = new MainWindow(m_settings, m_bookmarks, m_history, m_downloads, m_importManager, m_mozilla, m_google, m_storeInstaller, m_privateMode);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->show();
    if (w->m_tabs->count() == 1) w->closeTab(0);
    w->createTab(userInputToUrl(url), true, pinned);
    closeTab(index);
}

void MainWindow::closeTab(int index) {
    if (index < 0 || index >= m_tabs->count()) return;
    if (auto *view = qobject_cast<BrowserView *>(m_tabs->widget(index))) {
        const QString url = displayUrlForView(view);
        if (!url.isEmpty()) m_closedTabs.push_back(url);
    }
    auto *widget = m_tabs->widget(index);
    m_tabs->removeTab(index);
    widget->deleteLater();
    if (m_tabs->count() == 0) createTab(QUrl(QStringLiteral("qbrowse://home")), true);
    updateWindowTitle();
}

void MainWindow::reopenClosedTab() {
    if (m_closedTabs.isEmpty()) return;
    createTab(userInputToUrl(m_closedTabs.takeLast()), true);
}

void MainWindow::duplicateCurrentTab() {
    if (auto *v = currentView()) createTab(userInputToUrl(displayUrlForView(v)), true, v->property("pinned").toBool());
}

void MainWindow::toggleMuteCurrentTab() {
    if (auto *v = currentView()) v->page()->setAudioMuted(!v->page()->isAudioMuted());
}

void MainWindow::togglePinCurrentTab() {
    if (auto *v = currentView()) {
        v->setProperty("pinned", !v->property("pinned").toBool());
        const int old = m_tabs->currentIndex();
        if (v->property("pinned").toBool()) {
            int pos = 0; for (int i = 0; i < m_tabs->count(); ++i) if (m_tabs->widget(i)->property("pinned").toBool()) pos = i + 1;
            m_tabs->tabBar()->moveTab(old, pos);
        }
        updateTabVisual(m_tabs->indexOf(v));
    }
}

void MainWindow::showCommandPalette() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Командная палитра"));
    dlg.resize(720, 460);
    auto *layout = new QVBoxLayout(&dlg);
    auto *search = new QLineEdit; search->setPlaceholderText(QStringLiteral("Напиши команду, URL или имя вкладки"));
    auto *list = new QListWidget;
    layout->addWidget(search); layout->addWidget(list, 1);
    struct Entry { QString title; QString value; };
    QVector<Entry> items;
    items.push_back({QStringLiteral("Новая вкладка"), QStringLiteral("__newtab")});
    items.push_back({QStringLiteral("Настройки"), QStringLiteral("__settings")});
    items.push_back({QStringLiteral("История"), QStringLiteral("__history")});
    items.push_back({QStringLiteral("Закладки"), QStringLiteral("__bookmarks")});
    for (int i = 0; i < m_tabs->count(); ++i) if (auto *v = qobject_cast<BrowserView *>(m_tabs->widget(i))) items.push_back({v->title().isEmpty() ? QStringLiteral("Вкладка") : v->title(), displayUrlForView(v)});
    for (const auto &b : m_bookmarks->entries()) items.push_back({QStringLiteral("Закладка: ") + b.title, b.url});
    auto refill = [&]() {
        list->clear();
        const QString n = search->text().trimmed().toLower();
        for (const auto &e : items) {
            if (!n.isEmpty() && !(e.title + QStringLiteral(" ") + e.value).toLower().contains(n)) continue;
            auto *it = new QListWidgetItem(e.title + QStringLiteral("\n") + e.value);
            it->setData(Qt::UserRole, e.value);
            list->addItem(it);
        }
        if (list->count() > 0) list->setCurrentRow(0);
    };
    connect(search, &QLineEdit::textChanged, &dlg, [&](const QString &) { refill(); });
    connect(list, &QListWidget::itemActivated, &dlg, [&, this](QListWidgetItem *it) {
        const QString value = it->data(Qt::UserRole).toString();
        if (value == QStringLiteral("__newtab")) createTab(QUrl(QStringLiteral("qbrowse://home")), true);
        else if (value == QStringLiteral("__settings")) { ensureSettingsDialog(); m_settingsDialog->show(); }
        else if (value == QStringLiteral("__history")) m_historyDialog->show();
        else if (value == QStringLiteral("__bookmarks")) m_bookmarksDialog->show();
        else navigate(currentView(), userInputToUrl(value));
        dlg.accept();
    });
    refill();
    dlg.exec();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSession();
    QMainWindow::closeEvent(event);
}
