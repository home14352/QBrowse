#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QTcpServer>

class SettingsManager;

class OAuthAccountService : public QObject {
    Q_OBJECT
public:
    OAuthAccountService(SettingsManager *settings,
                        QString prefix,
                        QString displayName,
                        QString authUrl,
                        QString tokenUrl,
                        QString userInfoUrl,
                        QString defaultScopes,
                        QObject *parent = nullptr);

    QString displayName() const;
    QString status() const;
    QString email() const;
    QString lastSyncAt() const;
    QString lastError() const;
    bool isSignedIn() const;
    QString prefix() const;

public slots:
    void beginLogin();
    void logout();
    void syncNow();
    void fillDefaults();

signals:
    void signedIn(const QString &email);
    void signedOut();
    void error(const QString &message);
    void statusChanged();

private:
    void handleIncomingCallback();
    void exchangeAuthorizationCode(const QString &code);
    void fetchUserInfo(bool userInitiated);
    QString key(const QString &suffix) const;
    QString str(const QString &suffix) const;
    void set(const QString &suffix, const QVariant &value);
    void persistStatus(const QString &value);
    void persistError(const QString &value);
    QByteArray randomBytes(int count) const;
    QString base64Url(const QByteArray &bytes) const;

    SettingsManager *m_settings;
    QString m_prefix;
    QString m_displayName;
    QString m_defaultAuthUrl;
    QString m_defaultTokenUrl;
    QString m_defaultUserInfoUrl;
    QString m_defaultScopes;
    mutable QNetworkAccessManager m_network;
    QTcpServer m_server;
    QString m_state;
    QString m_codeVerifier;
};
