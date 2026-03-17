#include "RequestInterceptor.h"
#include "SettingsManager.h"
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QWebEngineUrlRequestInfo>
#include <QRegularExpression>


RequestInterceptor::RequestInterceptor(SettingsManager *settings, QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
    , m_settings(settings) {
    reload();
}

void RequestInterceptor::reload() {
    m_blockedHosts.clear();
    if (!m_settings->boolValue(QStringLiteral("privacy/enableHostBlocklist"), false)) return;
    const QString path = m_settings->stringValue(QStringLiteral("privacy/blockedHostsPath"));
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            const QString host = parts.last().trimmed();
            if (!host.isEmpty() && host != QStringLiteral("localhost")) m_blockedHosts.insert(host.toLower());
        }
    }
}

void RequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {
    const QString host = info.requestUrl().host().toLower();
    if (m_settings->boolValue(QStringLiteral("privacy/doNotTrack"), true)) {
        info.setHttpHeader("DNT", "1");
    }
    if (m_blockedHosts.contains(host)) {
        info.block(true);
        return;
    }
    const QVariantMap overrides = m_settings->mapValue(QStringLiteral("advanced/siteUserAgentOverrides"));
    if (overrides.contains(host)) {
        info.setHttpHeader("User-Agent", overrides.value(host).toString().toUtf8());
    }
}
