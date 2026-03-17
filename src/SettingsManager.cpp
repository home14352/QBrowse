#include "SettingsManager.h"
#include "AppPaths.h"
#include <QDir>

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings(AppPaths::settingsFile(), QSettings::IniFormat) {
    AppPaths::ensureAll();
    ensureDefaults();
}

QString SettingsManager::fileName() const { return m_settings.fileName(); }
QString SettingsManager::stringValue(const QString &key, const QString &fallback) const { return m_settings.value(key, fallback).toString(); }
bool SettingsManager::boolValue(const QString &key, bool fallback) const { return m_settings.value(key, fallback).toBool(); }
int SettingsManager::intValue(const QString &key, int fallback) const { return m_settings.value(key, fallback).toInt(); }
QStringList SettingsManager::stringListValue(const QString &key) const { return m_settings.value(key).toStringList(); }
QVariantMap SettingsManager::mapValue(const QString &key) const { return m_settings.value(key).toMap(); }
QVariant SettingsManager::value(const QString &key, const QVariant &fallback) const { return m_settings.value(key, fallback); }
void SettingsManager::setValue(const QString &key, const QVariant &value) { m_settings.setValue(key, value); emit changed(); }
void SettingsManager::sync() { m_settings.sync(); }

void SettingsManager::ensureDefaults() {
    const QList<QPair<QString, QVariant>> defaults = {
        {QStringLiteral("ui/language"), QStringLiteral("ru")},
        {QStringLiteral("general/homepage"), QStringLiteral("qbrowse://home")},
        {QStringLiteral("general/searchEngine"), QStringLiteral("DuckDuckGo")},
        {QStringLiteral("general/downloadPath"), AppPaths::downloadsDir()},
        {QStringLiteral("general/restoreSession"), true},
        {QStringLiteral("appearance/preset"), QStringLiteral("system")},
        {QStringLiteral("appearance/verticalTabs"), true},
        {QStringLiteral("appearance/iconOnlyVerticalTabs"), true},
        {QStringLiteral("appearance/compactMode"), false},
        {QStringLiteral("appearance/useSystemPalette"), true},
        {QStringLiteral("appearance/activeThemeId"), QString()},
        {QStringLiteral("privacy/doNotTrack"), true},
        {QStringLiteral("privacy/blockThirdPartyCookies"), true},
        {QStringLiteral("privacy/javascriptEnabled"), true},
        {QStringLiteral("privacy/allowFirefoxStoreInstall"), false},
        {QStringLiteral("privacy/dohMode"), QStringLiteral("off")},
        {QStringLiteral("privacy/dohServers"), QString()},
        {QStringLiteral("advanced/userAgentSuffix"), QString()},
        {QStringLiteral("advanced/externalFlags"), QString()},
        {QStringLiteral("system/desktopFileId"), QStringLiteral("qbrowse.desktop")},
        {QStringLiteral("tabs/reopenClosedEnabled"), true},
        {QStringLiteral("bookmarks/showBar"), true}
    };
    for (const auto &pair : defaults) {
        if (!m_settings.contains(pair.first)) {
            m_settings.setValue(pair.first, pair.second);
        }
    }
    m_settings.sync();
}
