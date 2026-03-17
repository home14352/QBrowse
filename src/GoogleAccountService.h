#pragma once
#include "OAuthAccountService.h"
class GoogleAccountService : public OAuthAccountService {
    Q_OBJECT
public:
    explicit GoogleAccountService(SettingsManager *settings, QObject *parent = nullptr);
};
