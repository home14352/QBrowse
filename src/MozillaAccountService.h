#pragma once
#include "OAuthAccountService.h"
class MozillaAccountService : public OAuthAccountService {
    Q_OBJECT
public:
    explicit MozillaAccountService(SettingsManager *settings, QObject *parent = nullptr);
};
