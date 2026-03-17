#include "MozillaAccountService.h"
#include "SettingsManager.h"

MozillaAccountService::MozillaAccountService(SettingsManager *settings, QObject *parent)
    : OAuthAccountService(settings,
                          QStringLiteral("mozilla"),
                          QStringLiteral("Mozilla Account"),
                          QStringLiteral("https://accounts.firefox.com/authorization"),
                          QStringLiteral("https://oauth.accounts.firefox.com/v1/token"),
                          QStringLiteral("https://api.accounts.firefox.com/v1/account/profile"),
                          QStringLiteral("profile:email"),
                          parent) {}
