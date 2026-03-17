# QBrowse 0.4.0

QBrowse — нативный браузер на **C++/Qt6 WebEngine (Chromium)** для Linux.

## Что реализовано в этой версии

- русскоязычный интерфейс по умолчанию
- управление закладками: добавление, редактирование, удаление, папки, экспорт HTML, импорт JSON
- вертикальные вкладки
- режим вертикальных вкладок «только иконка», а название и URL показываются в tooltip при наведении
- вынос вкладки в новое окно
- собственная иконка браузера
- менеджер расширений
- установка Chromium-расширений из **Chrome Web Store** по ID/URL
- установка расширений/тем из **Firefox Add-ons** по slug/URL только после включения соответствующей галочки в настройках
- открытие popup-окна выбранного расширения
- менеджер тем и пресетов
- темы из локальных архивов, Chrome Web Store и Firefox Add-ons
- поддержка анимированных Firefox-тем как фонового ассета встроенных страниц QBrowse
- Mozilla Account и Google Account через внешний браузер + локальный OAuth callback
- запрос разрешений сайта: камера, микрофон, геолокация, уведомления, экран/окно
- просмотр и сброс сохранённых разрешений
- импорт из Firefox/Floorp/Chromium/Chrome/Brave/Vivaldi/Opera/Edge
- установка ярлыка/desktop file прямо из интерфейса
- кнопка «сделать браузером по умолчанию»
- handler внешних ссылок через `.desktop` и `xdg-settings` / `xdg-mime`
- настройки внешних DNS over HTTPS серверов
- bookmark bar, sidebar, command palette, дублирование вкладки, mute tab, pin tab, reopen closed tab, private window

## Сборка на Arch Linux

```bash
./scripts/bootstrap-arch.sh
./scripts/build-local.sh
```

Локальная установка:

```bash
./scripts/install-system.sh
```

## Важные замечания

1. Qt WebEngine использует Chromium-движок и helper-процессы, поэтому практичный формат установки — обычная системная установка с desktop file, а не «один полностью автономный ELF без всего».
2. Установка расширений из Chrome Web Store сделана через best-effort bridge: скачивание CRX, преобразование в ZIP и передача в `QWebEngineExtensionManager`.
3. Установка из Firefox Add-ons gated-настройкой, потому что Firefox и Chromium WebExtensions совместимы не полностью.
4. Настройки DoH применяются через Chromium flags на старте приложения.

## Структура проекта

См. `docs/PROJECT_TREE.txt`.
