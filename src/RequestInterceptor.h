#pragma once
#include <QSet>
#include <QWebEngineUrlRequestInterceptor>

class SettingsManager;

class RequestInterceptor : public QWebEngineUrlRequestInterceptor {
    Q_OBJECT
public:
    explicit RequestInterceptor(SettingsManager *settings, QObject *parent = nullptr);
    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    void reload();
private:
    SettingsManager *m_settings;
    QSet<QString> m_blockedHosts;
};
