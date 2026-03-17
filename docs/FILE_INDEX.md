# Индекс файлов проекта QBrowse 0.4.0

## Верхний уровень
- `CMakeLists.txt` — сборка проекта
- `README.md` — краткое описание и старт

## `src/`
- `main.cpp` — старт приложения, применение DoH/Chromium flags
- `MainWindow.*` — главное окно браузера, вкладки, toolbar, sidebar, session restore
- `Dialogs.*` — настройки, закладки, история, загрузки
- `StoreInstaller.*` — Chrome Web Store / Firefox Add-ons / desktop integration
- `ThemeManager.*` — темы, пресеты, установка тем из архивов
- `BrowserView.*` — вкладка-браузер, permission prompts, popup/new window hook
- `RequestInterceptor.*` — DNT, hosts blocklist, UA overrides
- `OAuthAccountService.*` — общий OAuth-клиент
- `MozillaAccountService.*` — Mozilla Account defaults
- `GoogleAccountService.*` — Google Account defaults
- `ImportManager.*` — импорт из браузеров
- `BookmarkStore.*` — закладки
- `HistoryStore.*` — история
- `DownloadManager.*` — загрузки
- `SettingsManager.*` — INI-настройки
- `AppPaths.*` — системные пути и каталоги

## `resources/`
- `resources.qrc` — qrc-ресурсы
- `logo/qbrowse-logo.svg` — логотип и иконка

## `packaging/`
- `qbrowse.desktop` — desktop entry + x-scheme handlers

## `scripts/`
- `bootstrap-arch.sh` — зависимости Arch
- `build-local.sh` — локальная сборка
- `install-system.sh` — установка пользователю

## `docs/`
- `BUILD_ARCH.md` — сборка на Arch
- `APPLIED_IMPROVEMENTS_0_4.md` — список внедрённых улучшений
- `PROJECT_TREE.txt` — дерево проекта
