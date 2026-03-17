#include "GoogleAccountService.h"
#include "SettingsManager.h"

GoogleAccountService::GoogleAccountService(SettingsManager *settings, QObject *parent)
    : OAuthAccountService(settings,
                          QStringLiteral("google"),
                          QStringLiteral("Google Account"),
                          QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"),
                          QStringLiteral("https://oauth2.googleapis.com/token"),
                          QStringLiteral("https://openidconnect.googleapis.com/v1/userinfo"),
                          QStringLiteral("openid email profile"),
                          parent) {}
