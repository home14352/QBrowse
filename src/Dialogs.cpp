#include "Dialogs.h"
#include "BookmarkStore.h"
#include "DownloadManager.h"
#include "GoogleAccountService.h"
#include "HistoryStore.h"
#include "MozillaAccountService.h"
#include "SettingsManager.h"
#include "OAuthAccountService.h"
#include "StoreInstaller.h"
#include "ThemeManager.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWebEngineExtensionInfo>
#include <QWebEngineExtensionManager>
#include <QWebEnginePermission>
#include <QWebEngineProfile>

namespace {
QTableWidget *createTable(const QStringList &headers) {
    auto *table = new QTableWidget;
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    return table;
}
QTableWidgetItem *item(const QString &text) { return new QTableWidgetItem(text); }
int selectedRow(QTableWidget *table) {
    const auto rows = table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}
QString permissionName(QWebEnginePermission::PermissionType type) {
    using T = QWebEnginePermission::PermissionType;
    switch (type) {
    case T::MediaAudioCapture: return QStringLiteral("Микрофон");
    case T::MediaVideoCapture: return QStringLiteral("Камера");
    case T::MediaAudioVideoCapture: return QStringLiteral("Камера + микрофон");
    case T::Geolocation: return QStringLiteral("Геолокация");
    case T::Notifications: return QStringLiteral("Уведомления");
    case T::ClipboardReadWrite: return QStringLiteral("Буфер обмена");
    case T::LocalFontsAccess: return QStringLiteral("Локальные шрифты");
    default: return QStringLiteral("Доступ");
    }
}
QString permissionState(QWebEnginePermission::State state) {
    switch (state) {
    case QWebEnginePermission::State::Granted: return QStringLiteral("Разрешено");
    case QWebEnginePermission::State::Denied: return QStringLiteral("Запрещено");
    default: return QStringLiteral("Спрашивать");
    }
}
}

BookmarksDialog::BookmarksDialog(BookmarkStore *store, QWidget *parent)
    : QDialog(parent), m_store(store) {
    setWindowTitle(QStringLiteral("Управление закладками"));
    resize(980, 560);
    auto *layout = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    m_searchEdit = new QLineEdit; m_searchEdit->setPlaceholderText(QStringLiteral("Поиск по закладкам"));
    auto *addBtn = new QPushButton(QStringLiteral("Добавить"));
    auto *editBtn = new QPushButton(QStringLiteral("Изменить"));
    auto *removeBtn = new QPushButton(QStringLiteral("Удалить"));
    auto *importBtn = new QPushButton(QStringLiteral("Импорт JSON"));
    auto *exportBtn = new QPushButton(QStringLiteral("Экспорт HTML"));
    top->addWidget(m_searchEdit, 1);
    top->addWidget(addBtn); top->addWidget(editBtn); top->addWidget(removeBtn); top->addWidget(importBtn); top->addWidget(exportBtn);
    layout->addLayout(top);
    m_table = createTable({QStringLiteral("Название"), QStringLiteral("URL"), QStringLiteral("Папка"), QStringLiteral("Теги"), QStringLiteral("Создано")});
    layout->addWidget(m_table, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *openBtn = buttons->addButton(QStringLiteral("Открыть"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { refresh(); });
    connect(openBtn, &QPushButton::clicked, this, [this]() { const int row = selectedRow(m_table); if (row >= 0) emit openUrlRequested(m_table->item(row, 1)->text()); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString url = QInputDialog::getText(this, QStringLiteral("Новая закладка"), QStringLiteral("URL:"), QLineEdit::Normal, QString(), &ok);
        if (!ok || url.trimmed().isEmpty()) return;
        const QString title = QInputDialog::getText(this, QStringLiteral("Новая закладка"), QStringLiteral("Название:"), QLineEdit::Normal, url, &ok);
        if (!ok) return;
        const QString folder = QInputDialog::getText(this, QStringLiteral("Новая закладка"), QStringLiteral("Папка:"), QLineEdit::Normal, QStringLiteral("Панель"), &ok);
        if (!ok) return;
        m_store->addBookmark(title, url, folder);
    });
    connect(editBtn, &QPushButton::clicked, this, [this]() {
        const int row = selectedRow(m_table); if (row < 0) return;
        auto entries = m_store->search(m_searchEdit->text());
        if (row >= entries.size()) return;
        BookmarkEntry e = entries[row];
        bool ok = false;
        e.title = QInputDialog::getText(this, QStringLiteral("Изменить"), QStringLiteral("Название:"), QLineEdit::Normal, e.title, &ok); if (!ok) return;
        e.url = QInputDialog::getText(this, QStringLiteral("Изменить"), QStringLiteral("URL:"), QLineEdit::Normal, e.url, &ok); if (!ok) return;
        e.folder = QInputDialog::getText(this, QStringLiteral("Изменить"), QStringLiteral("Папка:"), QLineEdit::Normal, e.folder, &ok); if (!ok) return;
        const auto all = m_store->entries();
        for (int i = 0; i < all.size(); ++i) {
            if (all[i].createdAt == entries[row].createdAt && all[i].url == entries[row].url) { m_store->updateAt(i, e); break; }
        }
    });
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        const int row = selectedRow(m_table); if (row < 0) return;
        auto filtered = m_store->search(m_searchEdit->text());
        const auto all = m_store->entries();
        for (int i = 0; i < all.size(); ++i) {
            if (all[i].createdAt == filtered[row].createdAt && all[i].url == filtered[row].url) { m_store->removeAt(i); break; }
        }
    });
    connect(importBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Импорт JSON"), QString(), QStringLiteral("JSON (*.json)"));
        if (!path.isEmpty()) m_store->importJson(path);
    });
    connect(exportBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Экспорт HTML"), QStringLiteral("qbrowse-bookmarks.html"), QStringLiteral("HTML (*.html)"));
        if (!path.isEmpty()) m_store->exportNetscapeHtml(path);
    });
    connect(m_store, &BookmarkStore::changed, this, &BookmarksDialog::refresh);
    refresh();
}

void BookmarksDialog::refresh() {
    const auto entries = m_store->search(m_searchEdit->text());
    m_table->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        m_table->setItem(row, 0, item(entries[row].title));
        m_table->setItem(row, 1, item(entries[row].url));
        m_table->setItem(row, 2, item(entries[row].folder));
        m_table->setItem(row, 3, item(entries[row].tags.join(QStringLiteral(", "))));
        m_table->setItem(row, 4, item(entries[row].createdAt));
    }
}

HistoryDialog::HistoryDialog(HistoryStore *store, QWidget *parent)
    : QDialog(parent), m_store(store) {
    setWindowTitle(QStringLiteral("История"));
    resize(980, 560);
    auto *layout = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    m_searchEdit = new QLineEdit; m_searchEdit->setPlaceholderText(QStringLiteral("Поиск по истории"));
    auto *clearBtn = new QPushButton(QStringLiteral("Очистить историю"));
    top->addWidget(m_searchEdit, 1); top->addWidget(clearBtn);
    layout->addLayout(top);
    m_table = createTable({QStringLiteral("Название"), QStringLiteral("URL"), QStringLiteral("Последний визит"), QStringLiteral("Визиты")});
    layout->addWidget(m_table, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *openBtn = buttons->addButton(QStringLiteral("Открыть"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);
    connect(openBtn, &QPushButton::clicked, this, [this]() { const int row = selectedRow(m_table); if (row >= 0) emit openUrlRequested(m_table->item(row, 1)->text()); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &) { refresh(); });
    connect(clearBtn, &QPushButton::clicked, this, [this]() { if (QMessageBox::question(this, QStringLiteral("QBrowse"), QStringLiteral("Очистить всю историю?")) == QMessageBox::Yes) m_store->clear(); });
    connect(m_store, &HistoryStore::changed, this, &HistoryDialog::refresh);
    refresh();
}

void HistoryDialog::refresh() {
    const auto entries = m_searchEdit->text().trimmed().isEmpty() ? m_store->recent() : m_store->search(m_searchEdit->text());
    m_table->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        m_table->setItem(row, 0, item(entries[row].title));
        m_table->setItem(row, 1, item(entries[row].url));
        m_table->setItem(row, 2, item(entries[row].visitedAt.toLocalTime().toString(Qt::ISODate)));
        m_table->setItem(row, 3, item(QString::number(entries[row].visitCount)));
    }
}

DownloadsDialog::DownloadsDialog(DownloadManager *manager, QWidget *parent)
    : QDialog(parent), m_manager(manager) {
    setWindowTitle(QStringLiteral("Загрузки"));
    resize(980, 420);
    auto *layout = new QVBoxLayout(this);
    m_table = createTable({QStringLiteral("Файл"), QStringLiteral("Состояние"), QStringLiteral("Прогресс"), QStringLiteral("Путь"), QStringLiteral("Источник")});
    layout->addWidget(m_table, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *openBtn = buttons->addButton(QStringLiteral("Открыть файл"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);
    connect(openBtn, &QPushButton::clicked, this, [this]() { const int row = selectedRow(m_table); if (row >= 0) m_manager->openPath(row); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(m_manager, &DownloadManager::changed, this, &DownloadsDialog::refresh);
    refresh();
}

void DownloadsDialog::refresh() {
    const auto entries = m_manager->entries();
    m_table->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        m_table->setItem(row, 0, item(entries[row].fileName));
        m_table->setItem(row, 1, item(entries[row].state));
        m_table->setItem(row, 2, item(QString::number(entries[row].progress) + QStringLiteral("%")));
        m_table->setItem(row, 3, item(entries[row].path));
        m_table->setItem(row, 4, item(entries[row].sourceUrl));
    }
}

SettingsDialog::SettingsDialog(SettingsManager *settings,
                               ImportManager *importManager,
                               MozillaAccountService *mozilla,
                               GoogleAccountService *google,
                               QWebEngineProfile *profile,
                               StoreInstaller *storeInstaller,
                               QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_importManager(importManager)
    , m_mozilla(mozilla)
    , m_google(google)
    , m_profile(profile)
    , m_storeInstaller(storeInstaller)
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    , m_extensionManager(profile ? profile->extensionManager() : nullptr)
#endif
{
    setWindowTitle(QStringLiteral("Настройки QBrowse"));
    resize(1100, 780);
    buildUi();
    loadFromSettings();
    refreshAccounts();
    refreshExtensions();
    refreshPermissions();
    refreshThemes();
}

void SettingsDialog::buildUi() {
    auto *root = new QVBoxLayout(this);
    m_tabs = new QTabWidget;
    root->addWidget(m_tabs, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Применить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Закрыть"));
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() { saveToSettings(); emit settingsApplied(); refreshThemes(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);

    // General
    auto *generalPage = new QWidget;
    auto *general = new QFormLayout(generalPage);
    m_languageCombo = new QComboBox; m_languageCombo->addItem(QStringLiteral("Русский"), QStringLiteral("ru")); m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_homepageEdit = new QLineEdit;
    m_searchEngineCombo = new QComboBox; m_searchEngineCombo->addItems({QStringLiteral("DuckDuckGo"), QStringLiteral("Google")});
    m_downloadPathEdit = new QLineEdit;
    auto *browseDownloads = new QPushButton(QStringLiteral("Выбрать..."));
    auto *downloadRow = new QHBoxLayout; downloadRow->addWidget(m_downloadPathEdit, 1); downloadRow->addWidget(browseDownloads);
    auto *downloadWidget = new QWidget; downloadWidget->setLayout(downloadRow);
    m_restoreSessionCheck = new QCheckBox(QStringLiteral("Восстанавливать вкладки при старте"));
    m_showBookmarksBarCheck = new QCheckBox(QStringLiteral("Показывать панель закладок"));
    general->addRow(QStringLiteral("Язык интерфейса"), m_languageCombo);
    general->addRow(QStringLiteral("Домашняя страница"), m_homepageEdit);
    general->addRow(QStringLiteral("Поисковик"), m_searchEngineCombo);
    general->addRow(QStringLiteral("Папка загрузок"), downloadWidget);
    general->addRow(QString(), m_restoreSessionCheck);
    general->addRow(QString(), m_showBookmarksBarCheck);
    connect(browseDownloads, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Папка загрузок"), m_downloadPathEdit->text());
        if (!dir.isEmpty()) m_downloadPathEdit->setText(dir);
    });
    m_tabs->addTab(generalPage, QStringLiteral("Общие"));

    // Appearance
    auto *appearancePage = new QWidget;
    auto *appearance = new QVBoxLayout(appearancePage);
    auto *appearanceForm = new QFormLayout;
    m_presetCombo = new QComboBox; m_presetCombo->addItem(QStringLiteral("System"), QStringLiteral("system")); m_presetCombo->addItem(QStringLiteral("Plasma-like"), QStringLiteral("plasma")); m_presetCombo->addItem(QStringLiteral("GNOME-like"), QStringLiteral("gnome")); m_presetCombo->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
    m_verticalTabsCheck = new QCheckBox(QStringLiteral("Вертикальные вкладки"));
    m_iconOnlyVerticalTabsCheck = new QCheckBox(QStringLiteral("Во вертикальном режиме показывать только иконку сайта"));
    m_compactModeCheck = new QCheckBox(QStringLiteral("Компактный режим интерфейса"));
    m_themeCombo = new QComboBox; m_themeCombo->addItem(QStringLiteral("Без темы"), QString());
    appearanceForm->addRow(QStringLiteral("Пресет"), m_presetCombo);
    appearanceForm->addRow(QStringLiteral("Установленная тема"), m_themeCombo);
    appearanceForm->addRow(QString(), m_verticalTabsCheck);
    appearanceForm->addRow(QString(), m_iconOnlyVerticalTabsCheck);
    appearanceForm->addRow(QString(), m_compactModeCheck);
    appearance->addLayout(appearanceForm);
    m_themeTable = createTable({QStringLiteral("ID"), QStringLiteral("Название"), QStringLiteral("Фон"), QStringLiteral("Анимация")});
    appearance->addWidget(m_themeTable, 1);
    auto *themeButtons = new QGridLayout;
    auto *localThemeBtn = new QPushButton(QStringLiteral("Установить тему из файла"));
    m_themeChromeStoreEdit = new QLineEdit; m_themeChromeStoreEdit->setPlaceholderText(QStringLiteral("ID/URL темы Chrome Web Store"));
    auto *chromeThemeBtn = new QPushButton(QStringLiteral("Тема из Chrome Web Store"));
    m_themeFirefoxStoreEdit = new QLineEdit; m_themeFirefoxStoreEdit->setPlaceholderText(QStringLiteral("slug/URL темы Firefox Add-ons"));
    auto *firefoxThemeBtn = new QPushButton(QStringLiteral("Тема из Firefox Add-ons"));
    themeButtons->addWidget(localThemeBtn, 0, 0, 1, 2);
    themeButtons->addWidget(m_themeChromeStoreEdit, 1, 0); themeButtons->addWidget(chromeThemeBtn, 1, 1);
    themeButtons->addWidget(m_themeFirefoxStoreEdit, 2, 0); themeButtons->addWidget(firefoxThemeBtn, 2, 1);
    appearance->addLayout(themeButtons);
    connect(localThemeBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Файл темы"), QString(), QStringLiteral("Theme/Archive (*.zip *.xpi *.crx);;All files (*)"));
        if (path.isEmpty()) return; QString msg; ThemeManager::installThemePackage(path, QStringLiteral("local"), &msg); QMessageBox::information(this, QStringLiteral("Темы"), msg); refreshThemes();
    });
    connect(chromeThemeBtn, &QPushButton::clicked, this, [this]() { QString msg; m_storeInstaller->installFromChromeStore(m_themeChromeStoreEdit->text(), m_profile, &msg); QMessageBox::information(this, QStringLiteral("Темы"), msg); refreshThemes(); });
    connect(firefoxThemeBtn, &QPushButton::clicked, this, [this]() { QString msg; m_storeInstaller->installFromFirefoxStore(m_themeFirefoxStoreEdit->text(), m_profile, &msg); QMessageBox::information(this, QStringLiteral("Темы"), msg); refreshThemes(); });
    m_tabs->addTab(appearancePage, QStringLiteral("Внешний вид"));

    // Extensions
    auto *extensionsPage = new QWidget;
    auto *extensions = new QVBoxLayout(extensionsPage);
    m_extensionsTable = createTable({QStringLiteral("Название"), QStringLiteral("ID"), QStringLiteral("Включено"), QStringLiteral("Путь/ошибка")});
    extensions->addWidget(new QLabel(QStringLiteral("Поддерживаются Chromium/WebExtensions. Установка из Firefox Add-ons включается отдельной галочкой ниже.")));
    extensions->addWidget(m_extensionsTable, 1);
    auto *extGrid = new QGridLayout;
    auto *localExtBtn = new QPushButton(QStringLiteral("Установить архив/папку"));
    m_chromeStoreEdit = new QLineEdit; m_chromeStoreEdit->setPlaceholderText(QStringLiteral("ID/URL расширения Chrome Web Store"));
    auto *chromeExtBtn = new QPushButton(QStringLiteral("Установить из Chrome Web Store"));
    m_firefoxStoreEdit = new QLineEdit; m_firefoxStoreEdit->setPlaceholderText(QStringLiteral("slug/URL расширения Firefox Add-ons"));
    auto *firefoxExtBtn = new QPushButton(QStringLiteral("Установить из Firefox Add-ons"));
    auto *popupBtn = new QPushButton(QStringLiteral("Открыть popup выбранного расширения"));
    auto *toggleBtn = new QPushButton(QStringLiteral("Вкл/выкл выбранное"));
    auto *removeBtn = new QPushButton(QStringLiteral("Удалить выбранное"));
    extGrid->addWidget(localExtBtn, 0, 0, 1, 2);
    extGrid->addWidget(m_chromeStoreEdit, 1, 0); extGrid->addWidget(chromeExtBtn, 1, 1);
    extGrid->addWidget(m_firefoxStoreEdit, 2, 0); extGrid->addWidget(firefoxExtBtn, 2, 1);
    extGrid->addWidget(popupBtn, 3, 0); extGrid->addWidget(toggleBtn, 3, 1); extGrid->addWidget(removeBtn, 4, 0);
    extensions->addLayout(extGrid);
    connect(localExtBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Архив расширения"), QString(), QStringLiteral("Extensions (*.zip *.xpi *.crx);;All files (*)"));
        if (path.isEmpty()) path = QFileDialog::getExistingDirectory(this, QStringLiteral("Папка расширения"));
        if (path.isEmpty()) return; QString msg; m_storeInstaller->installFromLocalArchive(path, m_profile, &msg); QMessageBox::information(this, QStringLiteral("Расширения"), msg); refreshExtensions(); refreshThemes();
    });
    connect(chromeExtBtn, &QPushButton::clicked, this, [this]() { QString msg; m_storeInstaller->installFromChromeStore(m_chromeStoreEdit->text(), m_profile, &msg); QMessageBox::information(this, QStringLiteral("Расширения"), msg); refreshExtensions(); refreshThemes(); });
    connect(firefoxExtBtn, &QPushButton::clicked, this, [this]() { QString msg; m_storeInstaller->installFromFirefoxStore(m_firefoxStoreEdit->text(), m_profile, &msg); QMessageBox::information(this, QStringLiteral("Расширения"), msg); refreshExtensions(); refreshThemes(); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    connect(toggleBtn, &QPushButton::clicked, this, [this]() {
        const int row = selectedRow(m_extensionsTable); if (row < 0 || !m_extensionManager) return; const QString id = m_extensionsTable->item(row, 1)->text();
        for (const auto &ext : m_extensionManager->extensions()) if (ext.id() == id) { m_extensionManager->setExtensionEnabled(ext, !ext.isEnabled()); break; }
        refreshExtensions();
    });
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        const int row = selectedRow(m_extensionsTable); if (row < 0 || !m_extensionManager) return; const QString id = m_extensionsTable->item(row, 1)->text();
        for (const auto &ext : m_extensionManager->extensions()) if (ext.id() == id) { m_extensionManager->uninstallExtension(ext); break; }
        refreshExtensions();
    });
    connect(popupBtn, &QPushButton::clicked, this, [this]() {
        const int row = selectedRow(m_extensionsTable); if (row < 0 || !m_extensionManager) return; const QString id = m_extensionsTable->item(row, 1)->text();
        for (const auto &ext : m_extensionManager->extensions()) if (ext.id() == id) { if (!m_storeInstaller->openExtensionPopup(ext, m_profile, this)) QMessageBox::information(this, QStringLiteral("Popup"), QStringLiteral("У расширения нет popup-окна.")); break; }
    });
#endif
    m_tabs->addTab(extensionsPage, QStringLiteral("Расширения"));

    // Privacy
    auto *privacyPage = new QWidget;
    auto *privacy = new QFormLayout(privacyPage);
    m_doNotTrackCheck = new QCheckBox(QStringLiteral("Отправлять Do Not Track"));
    m_blockThirdPartyCookiesCheck = new QCheckBox(QStringLiteral("Блокировать сторонние cookies"));
    m_javascriptCheck = new QCheckBox(QStringLiteral("Включить JavaScript"));
    m_enableHostsBlocklistCheck = new QCheckBox(QStringLiteral("Использовать hosts/blocklist"));
    m_hostsPathEdit = new QLineEdit;
    auto *hostsBtn = new QPushButton(QStringLiteral("Выбрать..."));
    auto *hostsRow = new QHBoxLayout; hostsRow->addWidget(m_hostsPathEdit, 1); hostsRow->addWidget(hostsBtn); auto *hostsWidget = new QWidget; hostsWidget->setLayout(hostsRow);
    m_allowFirefoxStoreInstallCheck = new QCheckBox(QStringLiteral("Разрешить установку из Firefox Add-ons"));
    m_dohModeCombo = new QComboBox; m_dohModeCombo->addItem(QStringLiteral("off"), QStringLiteral("off")); m_dohModeCombo->addItem(QStringLiteral("automatic"), QStringLiteral("automatic")); m_dohModeCombo->addItem(QStringLiteral("secure"), QStringLiteral("secure"));
    m_dohTemplatesEdit = new QLineEdit; m_dohTemplatesEdit->setPlaceholderText(QStringLiteral("https://dns.example/dns-query{?dns}  https://dns2.example/dns-query{?dns}"));
    privacy->addRow(QString(), m_doNotTrackCheck);
    privacy->addRow(QString(), m_blockThirdPartyCookiesCheck);
    privacy->addRow(QString(), m_javascriptCheck);
    privacy->addRow(QString(), m_enableHostsBlocklistCheck);
    privacy->addRow(QStringLiteral("Файл hosts/blocklist"), hostsWidget);
    privacy->addRow(QString(), m_allowFirefoxStoreInstallCheck);
    privacy->addRow(QStringLiteral("DNS over HTTPS mode"), m_dohModeCombo);
    privacy->addRow(QStringLiteral("Внешние DoH-серверы"), m_dohTemplatesEdit);
    connect(hostsBtn, &QPushButton::clicked, this, [this]() { const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Файл hosts"), QString(), QStringLiteral("All files (*)")); if (!path.isEmpty()) m_hostsPathEdit->setText(path); });
    m_tabs->addTab(privacyPage, QStringLiteral("Приватность и DNS"));

    // Accounts
    auto *accountsPage = new QWidget;
    auto *accounts = new QVBoxLayout(accountsPage);
    auto addAccountGroup = [&](const QString &title, QLineEdit *&clientId, QLineEdit *&clientSecret, QLineEdit *&status, QLineEdit *&email, QLineEdit *&error,
                               OAuthAccountService *service) {
        auto *box = new QGroupBox(title);
        auto *form = new QFormLayout(box);
        clientId = new QLineEdit; clientSecret = new QLineEdit; clientSecret->setEchoMode(QLineEdit::Password);
        status = new QLineEdit; email = new QLineEdit; error = new QLineEdit;
        status->setReadOnly(true); email->setReadOnly(true); error->setReadOnly(true);
        auto *row = new QHBoxLayout;
        auto *defaultsBtn = new QPushButton(QStringLiteral("Заполнить дефолты"));
        auto *loginBtn = new QPushButton(QStringLiteral("Войти"));
        auto *logoutBtn = new QPushButton(QStringLiteral("Выйти"));
        auto *syncBtn = new QPushButton(QStringLiteral("Синхронизировать"));
        row->addWidget(defaultsBtn); row->addWidget(loginBtn); row->addWidget(logoutBtn); row->addWidget(syncBtn);
        auto *rowW = new QWidget; rowW->setLayout(row);
        form->addRow(QStringLiteral("Client ID"), clientId);
        form->addRow(QStringLiteral("Client Secret"), clientSecret);
        form->addRow(QStringLiteral("Статус"), status);
        form->addRow(QStringLiteral("Email"), email);
        form->addRow(QStringLiteral("Последняя ошибка"), error);
        form->addRow(QString(), rowW);
        connect(defaultsBtn, &QPushButton::clicked, service, &OAuthAccountService::fillDefaults);
        connect(loginBtn, &QPushButton::clicked, service, &OAuthAccountService::beginLogin);
        connect(logoutBtn, &QPushButton::clicked, service, &OAuthAccountService::logout);
        connect(syncBtn, &QPushButton::clicked, service, &OAuthAccountService::syncNow);
        connect(service, &OAuthAccountService::statusChanged, this, &SettingsDialog::refreshAccounts);
        accounts->addWidget(box);
    };
    addAccountGroup(QStringLiteral("Mozilla Account"), m_mozillaClientId, m_mozillaClientSecret, m_mozillaStatus, m_mozillaEmail, m_mozillaError, m_mozilla);
    addAccountGroup(QStringLiteral("Google Account"), m_googleClientId, m_googleClientSecret, m_googleStatus, m_googleEmail, m_googleError, m_google);
    accounts->addStretch();
    m_tabs->addTab(accountsPage, QStringLiteral("Аккаунты"));

    // Permissions
    auto *permPage = new QWidget;
    auto *permLayout = new QVBoxLayout(permPage);
    m_permissionsTable = createTable({QStringLiteral("Origin"), QStringLiteral("Тип"), QStringLiteral("Состояние")});
    permLayout->addWidget(m_permissionsTable, 1);
    auto *resetPermBtn = new QPushButton(QStringLiteral("Сбросить выбранное разрешение"));
    permLayout->addWidget(resetPermBtn);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(resetPermBtn, &QPushButton::clicked, this, [this]() {
        const int row = selectedRow(m_permissionsTable); if (row < 0) return; const auto perms = m_profile->listAllPermissions(); if (row >= perms.size()) return; auto p = perms[row]; p.reset(); refreshPermissions();
    });
#else
    resetPermBtn->setEnabled(false);
#endif
    m_tabs->addTab(permPage, QStringLiteral("Разрешения"));

    // Import/System/Advanced
    auto *otherPage = new QWidget;
    auto *other = new QVBoxLayout(otherPage);
    auto *importGrid = new QGridLayout;
    QStringList labels = {QStringLiteral("Firefox"), QStringLiteral("Floorp"), QStringLiteral("Chromium"), QStringLiteral("Chrome"), QStringLiteral("Brave"), QStringLiteral("Vivaldi"), QStringLiteral("Opera"), QStringLiteral("Edge")};
    QList<BrowserSource> sources = {BrowserSource::Firefox, BrowserSource::Floorp, BrowserSource::Chromium, BrowserSource::Chrome, BrowserSource::Brave, BrowserSource::Vivaldi, BrowserSource::Opera, BrowserSource::Edge};
    for (int i = 0; i < labels.size(); ++i) {
        auto *btn = new QPushButton(QStringLiteral("Импорт: ") + labels[i]);
        importGrid->addWidget(btn, i / 2, i % 2);
        connect(btn, &QPushButton::clicked, this, [this, i, sources]() { appendImportReport(m_importManager->importBrowser(sources[i])); });
    }
    auto *autoBtn = new QPushButton(QStringLiteral("Авто-импорт"));
    importGrid->addWidget(autoBtn, 4, 0, 1, 2);
    connect(autoBtn, &QPushButton::clicked, this, [this]() { appendImportReport(m_importManager->importAuto()); });
    other->addLayout(importGrid);
    auto *sysRow = new QHBoxLayout;
    auto *installDesktopBtn = new QPushButton(QStringLiteral("Установить ярлык/desktop file"));
    auto *defaultBtn = new QPushButton(QStringLiteral("Сделать браузером по умолчанию"));
    sysRow->addWidget(installDesktopBtn); sysRow->addWidget(defaultBtn);
    other->addLayout(sysRow);
    connect(installDesktopBtn, &QPushButton::clicked, this, [this]() { QString msg; m_storeInstaller->installUserDesktopFile(QCoreApplication::applicationFilePath(), &msg); QMessageBox::information(this, QStringLiteral("QBrowse"), msg); });
    connect(defaultBtn, &QPushButton::clicked, this, [this]() { QString msg; m_storeInstaller->makeDefaultBrowser(&msg); QMessageBox::information(this, QStringLiteral("QBrowse"), msg); });
    auto *advForm = new QFormLayout;
    m_userAgentSuffixEdit = new QLineEdit;
    m_externalFlagsEdit = new QLineEdit; m_externalFlagsEdit->setPlaceholderText(QStringLiteral("Дополнительные Chromium-флаги"));
    advForm->addRow(QStringLiteral("User-Agent suffix"), m_userAgentSuffixEdit);
    advForm->addRow(QStringLiteral("Внешние флаги Chromium"), m_externalFlagsEdit);
    other->addLayout(advForm);
    m_log = new QTextEdit; m_log->setReadOnly(true);
    other->addWidget(m_log, 1);
    m_tabs->addTab(otherPage, QStringLiteral("Импорт / Система / Доп."));
}

void SettingsDialog::loadFromSettings() {
    const QString lang = m_settings->stringValue(QStringLiteral("ui/language"), QStringLiteral("ru"));
    m_languageCombo->setCurrentIndex(lang == QStringLiteral("en") ? 1 : 0);
    m_homepageEdit->setText(m_settings->stringValue(QStringLiteral("general/homepage"), QStringLiteral("qbrowse://home")));
    m_searchEngineCombo->setCurrentText(m_settings->stringValue(QStringLiteral("general/searchEngine"), QStringLiteral("DuckDuckGo")));
    m_downloadPathEdit->setText(m_settings->stringValue(QStringLiteral("general/downloadPath")));
    m_restoreSessionCheck->setChecked(m_settings->boolValue(QStringLiteral("general/restoreSession"), true));
    m_showBookmarksBarCheck->setChecked(m_settings->boolValue(QStringLiteral("bookmarks/showBar"), true));
    for (int i = 0; i < m_presetCombo->count(); ++i) if (m_presetCombo->itemData(i).toString() == m_settings->stringValue(QStringLiteral("appearance/preset"), QStringLiteral("system"))) m_presetCombo->setCurrentIndex(i);
    m_verticalTabsCheck->setChecked(m_settings->boolValue(QStringLiteral("appearance/verticalTabs"), true));
    m_iconOnlyVerticalTabsCheck->setChecked(m_settings->boolValue(QStringLiteral("appearance/iconOnlyVerticalTabs"), true));
    m_compactModeCheck->setChecked(m_settings->boolValue(QStringLiteral("appearance/compactMode"), false));
    m_doNotTrackCheck->setChecked(m_settings->boolValue(QStringLiteral("privacy/doNotTrack"), true));
    m_blockThirdPartyCookiesCheck->setChecked(m_settings->boolValue(QStringLiteral("privacy/blockThirdPartyCookies"), true));
    m_javascriptCheck->setChecked(m_settings->boolValue(QStringLiteral("privacy/javascriptEnabled"), true));
    m_enableHostsBlocklistCheck->setChecked(m_settings->boolValue(QStringLiteral("privacy/enableHostBlocklist"), false));
    m_hostsPathEdit->setText(m_settings->stringValue(QStringLiteral("privacy/blockedHostsPath")));
    m_allowFirefoxStoreInstallCheck->setChecked(m_settings->boolValue(QStringLiteral("privacy/allowFirefoxStoreInstall"), false));
    for (int i = 0; i < m_dohModeCombo->count(); ++i) if (m_dohModeCombo->itemData(i).toString() == m_settings->stringValue(QStringLiteral("privacy/dohMode"), QStringLiteral("off"))) m_dohModeCombo->setCurrentIndex(i);
    m_dohTemplatesEdit->setText(m_settings->stringValue(QStringLiteral("privacy/dohServers")));
    m_userAgentSuffixEdit->setText(m_settings->stringValue(QStringLiteral("advanced/userAgentSuffix")));
    m_externalFlagsEdit->setText(m_settings->stringValue(QStringLiteral("advanced/externalFlags")));
    m_mozillaClientId->setText(m_settings->stringValue(QStringLiteral("mozilla/clientId")));
    m_mozillaClientSecret->setText(m_settings->stringValue(QStringLiteral("mozilla/clientSecret")));
    m_googleClientId->setText(m_settings->stringValue(QStringLiteral("google/clientId")));
    m_googleClientSecret->setText(m_settings->stringValue(QStringLiteral("google/clientSecret")));
    refreshThemes();
}

void SettingsDialog::saveToSettings() {
    m_settings->setValue(QStringLiteral("ui/language"), m_languageCombo->currentData().toString());
    m_settings->setValue(QStringLiteral("general/homepage"), m_homepageEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("general/searchEngine"), m_searchEngineCombo->currentText());
    m_settings->setValue(QStringLiteral("general/downloadPath"), m_downloadPathEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("general/restoreSession"), m_restoreSessionCheck->isChecked());
    m_settings->setValue(QStringLiteral("bookmarks/showBar"), m_showBookmarksBarCheck->isChecked());
    m_settings->setValue(QStringLiteral("appearance/preset"), m_presetCombo->currentData().toString());
    m_settings->setValue(QStringLiteral("appearance/verticalTabs"), m_verticalTabsCheck->isChecked());
    m_settings->setValue(QStringLiteral("appearance/iconOnlyVerticalTabs"), m_iconOnlyVerticalTabsCheck->isChecked());
    m_settings->setValue(QStringLiteral("appearance/compactMode"), m_compactModeCheck->isChecked());
    m_settings->setValue(QStringLiteral("appearance/activeThemeId"), m_themeCombo->currentData().toString());
    m_settings->setValue(QStringLiteral("privacy/doNotTrack"), m_doNotTrackCheck->isChecked());
    m_settings->setValue(QStringLiteral("privacy/blockThirdPartyCookies"), m_blockThirdPartyCookiesCheck->isChecked());
    m_settings->setValue(QStringLiteral("privacy/javascriptEnabled"), m_javascriptCheck->isChecked());
    m_settings->setValue(QStringLiteral("privacy/enableHostBlocklist"), m_enableHostsBlocklistCheck->isChecked());
    m_settings->setValue(QStringLiteral("privacy/blockedHostsPath"), m_hostsPathEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("privacy/allowFirefoxStoreInstall"), m_allowFirefoxStoreInstallCheck->isChecked());
    m_settings->setValue(QStringLiteral("privacy/dohMode"), m_dohModeCombo->currentData().toString());
    m_settings->setValue(QStringLiteral("privacy/dohServers"), m_dohTemplatesEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("advanced/userAgentSuffix"), m_userAgentSuffixEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("advanced/externalFlags"), m_externalFlagsEdit->text().trimmed());
    m_settings->setValue(QStringLiteral("mozilla/clientId"), m_mozillaClientId->text().trimmed());
    m_settings->setValue(QStringLiteral("mozilla/clientSecret"), m_mozillaClientSecret->text());
    m_settings->setValue(QStringLiteral("google/clientId"), m_googleClientId->text().trimmed());
    m_settings->setValue(QStringLiteral("google/clientSecret"), m_googleClientSecret->text());
    m_settings->sync();
}

void SettingsDialog::refreshAccounts() {
    m_mozillaStatus->setText(m_mozilla->status());
    m_mozillaEmail->setText(m_mozilla->email());
    m_mozillaError->setText(m_mozilla->lastError());
    m_googleStatus->setText(m_google->status());
    m_googleEmail->setText(m_google->email());
    m_googleError->setText(m_google->lastError());
}

void SettingsDialog::refreshExtensions() {
    if (!m_extensionsTable) return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    if (!m_extensionManager) { m_extensionsTable->setRowCount(0); return; }
    const auto exts = m_extensionManager->extensions();
    m_extensionsTable->setRowCount(exts.size());
    for (int row = 0; row < exts.size(); ++row) {
        const auto &e = exts[row];
        m_extensionsTable->setItem(row, 0, item(e.name()));
        m_extensionsTable->setItem(row, 1, item(e.id()));
        m_extensionsTable->setItem(row, 2, item(e.isEnabled() ? QStringLiteral("Да") : QStringLiteral("Нет")));
        m_extensionsTable->setItem(row, 3, item(e.error().isEmpty() ? e.path() : e.error()));
    }
#else
    m_extensionsTable->setRowCount(0);
#endif
}

void SettingsDialog::refreshPermissions() {
    if (!m_permissionsTable) return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    const auto perms = m_profile->listAllPermissions();
    m_permissionsTable->setRowCount(perms.size());
    for (int row = 0; row < perms.size(); ++row) {
        const auto &p = perms[row];
        m_permissionsTable->setItem(row, 0, item(p.origin().toString()));
        m_permissionsTable->setItem(row, 1, item(permissionName(p.permissionType())));
        m_permissionsTable->setItem(row, 2, item(permissionState(p.state())));
    }
#else
    m_permissionsTable->setRowCount(0);
#endif
}

void SettingsDialog::refreshThemes() {
    const auto themes = ThemeManager::installedThemes();
    const QString active = m_settings->stringValue(QStringLiteral("appearance/activeThemeId"));
    m_themeCombo->clear();
    m_themeCombo->addItem(QStringLiteral("Без темы"), QString());
    m_themeTable->setRowCount(themes.size());
    for (int row = 0; row < themes.size(); ++row) {
        const auto &t = themes[row];
        m_themeCombo->addItem(t.title, t.id);
        if (t.id == active) m_themeCombo->setCurrentIndex(m_themeCombo->count() - 1);
        m_themeTable->setItem(row, 0, item(t.id));
        m_themeTable->setItem(row, 1, item(t.title));
        m_themeTable->setItem(row, 2, item(t.backgroundImage));
        m_themeTable->setItem(row, 3, item(t.animated ? QStringLiteral("Да") : QStringLiteral("Нет")));
    }
}

void SettingsDialog::appendImportReport(const ImportReport &report) {
    m_log->append(QStringLiteral("=== Импорт ==="));
    m_log->append(report.summary());
    m_log->append(QString());
}
