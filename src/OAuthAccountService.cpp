#include "OAuthAccountService.h"
#include "SettingsManager.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

OAuthAccountService::OAuthAccountService(SettingsManager *settings,
                                         QString prefix,
                                         QString displayName,
                                         QString authUrl,
                                         QString tokenUrl,
                                         QString userInfoUrl,
                                         QString defaultScopes,
                                         QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_prefix(std::move(prefix))
    , m_displayName(std::move(displayName))
    , m_defaultAuthUrl(std::move(authUrl))
    , m_defaultTokenUrl(std::move(tokenUrl))
    , m_defaultUserInfoUrl(std::move(userInfoUrl))
    , m_defaultScopes(std::move(defaultScopes)) {
    fillDefaults();
    connect(&m_server, &QTcpServer::newConnection, this, &OAuthAccountService::handleIncomingCallback);
}

QString OAuthAccountService::displayName() const { return m_displayName; }
QString OAuthAccountService::prefix() const { return m_prefix; }
QString OAuthAccountService::key(const QString &suffix) const { return m_prefix + QStringLiteral("/") + suffix; }
QString OAuthAccountService::str(const QString &suffix) const { return m_settings->stringValue(key(suffix)); }
void OAuthAccountService::set(const QString &suffix, const QVariant &value) { m_settings->setValue(key(suffix), value); }
QString OAuthAccountService::status() const { return str(QStringLiteral("status")); }
QString OAuthAccountService::email() const { return str(QStringLiteral("email")); }
QString OAuthAccountService::lastSyncAt() const { return str(QStringLiteral("lastSyncAt")); }
QString OAuthAccountService::lastError() const { return str(QStringLiteral("lastError")); }
bool OAuthAccountService::isSignedIn() const { return !str(QStringLiteral("accessToken")).isEmpty(); }

void OAuthAccountService::fillDefaults() {
    if (str(QStringLiteral("clientId")).isEmpty()) set(QStringLiteral("clientId"), QString());
    if (str(QStringLiteral("clientSecret")).isEmpty()) set(QStringLiteral("clientSecret"), QString());
    if (str(QStringLiteral("authUrl")).isEmpty()) set(QStringLiteral("authUrl"), m_defaultAuthUrl);
    if (str(QStringLiteral("tokenUrl")).isEmpty()) set(QStringLiteral("tokenUrl"), m_defaultTokenUrl);
    if (str(QStringLiteral("userInfoUrl")).isEmpty()) set(QStringLiteral("userInfoUrl"), m_defaultUserInfoUrl);
    if (str(QStringLiteral("scopes")).isEmpty()) set(QStringLiteral("scopes"), m_defaultScopes);
    if (str(QStringLiteral("redirectUri")).isEmpty()) set(QStringLiteral("redirectUri"), QStringLiteral("http://127.0.0.1:38765/callback"));
    if (str(QStringLiteral("deviceName")).isEmpty()) set(QStringLiteral("deviceName"), QStringLiteral("QBrowse on Linux"));
    if (str(QStringLiteral("deviceType")).isEmpty()) set(QStringLiteral("deviceType"), QStringLiteral("desktop"));
    if (str(QStringLiteral("status")).isEmpty()) set(QStringLiteral("status"), QStringLiteral("Не выполнен вход"));
    m_settings->sync();
}

void OAuthAccountService::persistStatus(const QString &value) { set(QStringLiteral("status"), value); m_settings->sync(); emit statusChanged(); }
void OAuthAccountService::persistError(const QString &value) { set(QStringLiteral("lastError"), value); m_settings->sync(); emit statusChanged(); }

QByteArray OAuthAccountService::randomBytes(int count) const {
    QByteArray bytes(count, Qt::Uninitialized);
    for (int i = 0; i < count; ++i) bytes[i] = char(QRandomGenerator::global()->bounded(0, 256));
    return bytes;
}
QString OAuthAccountService::base64Url(const QByteArray &bytes) const { return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)); }

void OAuthAccountService::beginLogin() {
    const QString clientId = str(QStringLiteral("clientId"));
    const QString clientSecret = str(QStringLiteral("clientSecret"));
    const QString redirectUri = str(QStringLiteral("redirectUri"));
    const QString authUrl = str(QStringLiteral("authUrl"));
    if (clientId.isEmpty() || clientSecret.isEmpty()) {
        const QString text = QStringLiteral("Для %1 нужно заполнить client_id и client_secret.").arg(m_displayName);
        persistError(text); persistStatus(text); emit error(text); return;
    }
    const QUrl redirect(redirectUri);
    if (!redirect.isValid() || redirect.scheme() != QStringLiteral("http")) {
        const QString text = QStringLiteral("Redirect URI должен быть локальным http:// URL.");
        persistError(text); persistStatus(text); emit error(text); return;
    }
    if (m_server.isListening()) m_server.close();
    if (!m_server.listen(QHostAddress::LocalHost, redirect.port(38765))) {
        const QString text = QStringLiteral("Не удалось открыть локальный callback-порт.");
        persistError(text); persistStatus(text); emit error(text); return;
    }
    m_state = base64Url(randomBytes(24));
    m_codeVerifier = base64Url(randomBytes(32));
    const QString challenge = base64Url(QCryptographicHash::hash(m_codeVerifier.toUtf8(), QCryptographicHash::Sha256));
    QUrl url(authUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId);
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), str(QStringLiteral("scopes")));
    query.addQueryItem(QStringLiteral("state"), m_state);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    query.addQueryItem(QStringLiteral("code_challenge"), challenge);
    url.setQuery(query);
    persistError(QString());
    persistStatus(QStringLiteral("Ожидание входа в %1...").arg(m_displayName));
    QDesktopServices::openUrl(url);
}

void OAuthAccountService::logout() {
    set(QStringLiteral("accessToken"), QString());
    set(QStringLiteral("refreshToken"), QString());
    set(QStringLiteral("email"), QString());
    set(QStringLiteral("lastSyncAt"), QString());
    set(QStringLiteral("lastError"), QString());
    persistStatus(QStringLiteral("Выход выполнен"));
    emit signedOut();
}

void OAuthAccountService::syncNow() {
    if (!isSignedIn()) {
        const QString text = QStringLiteral("Сначала нужно войти в %1.").arg(m_displayName);
        persistError(text); persistStatus(text); emit error(text); return;
    }
    persistStatus(QStringLiteral("Обновление профиля %1...").arg(m_displayName));
    fetchUserInfo(true);
}

void OAuthAccountService::handleIncomingCallback() {
    auto *socket = m_server.nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        const QByteArray request = socket->readAll();
        const QList<QByteArray> lines = request.split('\n');
        if (lines.isEmpty()) return;
        const QList<QByteArray> parts = lines.first().trimmed().split(' ');
        if (parts.size() < 2) return;
        const QUrl url(QStringLiteral("http://127.0.0.1") + QString::fromUtf8(parts[1]));
        const QUrlQuery query(url);
        const QString code = query.queryItemValue(QStringLiteral("code"));
        const QString state = query.queryItemValue(QStringLiteral("state"));
        const QString errorCode = query.queryItemValue(QStringLiteral("error"));
        QByteArray response;
        if (!errorCode.isEmpty() || state != m_state || code.isEmpty()) {
            const QString text = errorCode.isEmpty() ? QStringLiteral("Ошибка callback OAuth.") : errorCode;
            persistError(text); persistStatus(text); emit error(text);
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h2>QBrowse</h2><p>Вход не завершён.</p></body></html>";
        } else {
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h2>QBrowse</h2><p>Можно закрыть окно и вернуться в браузер.</p></body></html>";
            exchangeAuthorizationCode(code);
        }
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();
        m_server.close();
    });
}

void OAuthAccountService::exchangeAuthorizationCode(const QString &code) {
    QNetworkRequest request{QUrl(str(QStringLiteral("tokenUrl")))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), str(QStringLiteral("clientId")));
    query.addQueryItem(QStringLiteral("client_secret"), str(QStringLiteral("clientSecret")));
    query.addQueryItem(QStringLiteral("code"), code);
    query.addQueryItem(QStringLiteral("redirect_uri"), str(QStringLiteral("redirectUri")));
    query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    query.addQueryItem(QStringLiteral("code_verifier"), m_codeVerifier);
    auto *reply = m_network.post(request, query.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString text = QStringLiteral("Ошибка обмена token: %1").arg(QString::fromUtf8(body));
            persistError(text); persistStatus(text); emit error(text); return;
        }
        const auto obj = QJsonDocument::fromJson(body).object();
        const QString accessToken = obj.value(QStringLiteral("access_token")).toString();
        const QString refreshToken = obj.value(QStringLiteral("refresh_token")).toString();
        if (accessToken.isEmpty()) {
            const QString text = QStringLiteral("Сервер не вернул access_token.");
            persistError(text); persistStatus(text); emit error(text); return;
        }
        set(QStringLiteral("accessToken"), accessToken);
        set(QStringLiteral("refreshToken"), refreshToken);
        set(QStringLiteral("lastError"), QString());
        m_settings->sync();
        persistStatus(QStringLiteral("Вход выполнен, читаю профиль..."));
        fetchUserInfo(false);
    });
}

void OAuthAccountService::fetchUserInfo(bool userInitiated) {
    const QString accessToken = str(QStringLiteral("accessToken"));
    if (accessToken.isEmpty()) return;
    QNetworkRequest request{QUrl(str(QStringLiteral("userInfoUrl")))};
    request.setRawHeader("Authorization", QByteArray("Bearer ") + accessToken.toUtf8());
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, userInitiated]() {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString text = QStringLiteral("Ошибка чтения профиля: %1").arg(QString::fromUtf8(body));
            persistError(text); persistStatus(text); emit error(text); return;
        }
        const auto obj = QJsonDocument::fromJson(body).object();
        const QString emailValue = obj.value(QStringLiteral("email")).toString();
        set(QStringLiteral("email"), emailValue);
        set(QStringLiteral("lastSyncAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
        set(QStringLiteral("lastError"), QString());
        m_settings->sync();
        const QString statusText = userInitiated
            ? QStringLiteral("Синхронизация профиля %1 завершена").arg(m_displayName)
            : QStringLiteral("Вход выполнен: %1").arg(emailValue.isEmpty() ? m_displayName : emailValue);
        persistStatus(statusText);
        emit signedIn(emailValue);
    });
}
