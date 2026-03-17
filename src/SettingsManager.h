#pragma once
#include <QObject>
#include <QSettings>
#include <QVariantMap>

class SettingsManager : public QObject {
    Q_OBJECT
public:
    explicit SettingsManager(QObject *parent = nullptr);
    QString fileName() const;
    QString stringValue(const QString &key, const QString &fallback = {}) const;
    bool boolValue(const QString &key, bool fallback = false) const;
    int intValue(const QString &key, int fallback = 0) const;
    QStringList stringListValue(const QString &key) const;
    QVariantMap mapValue(const QString &key) const;
    QVariant value(const QString &key, const QVariant &fallback = {}) const;
    void setValue(const QString &key, const QVariant &value);
    void sync();
    void ensureDefaults();
signals:
    void changed();
private:
    QSettings m_settings;
};
