#pragma once
#include <QObject>
#include <QSettings>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class UserPreferences : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY placesChanged)
    Q_PROPERTY(QVariantList recentPlaces READ recentPlaces NOTIFY placesChanged)
    Q_PROPERTY(QString profile READ profile WRITE setProfile NOTIFY displayChanged)
    Q_PROPERTY(QStringList visibleGroups READ visibleGroups WRITE setVisibleGroups NOTIFY displayChanged)
    Q_PROPERTY(QString windUnit READ windUnit WRITE setWindUnit NOTIFY displayChanged)
    Q_PROPERTY(bool utc READ utc WRITE setUtc NOTIFY displayChanged)
    Q_PROPERTY(bool showNight READ showNight WRITE setShowNight NOTIFY displayChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY displayChanged)
    Q_PROPERTY(QVariantList chartLayout READ chartLayout NOTIFY displayChanged)
    Q_PROPERTY(QVariantList chartCatalog READ chartCatalog CONSTANT)
public:
    explicit UserPreferences(QObject *parent = nullptr);
    QVariantList favorites() const;
    QVariantList recentPlaces() const;
    QString profile() const;
    QStringList visibleGroups() const;
    QString windUnit() const;
    bool utc() const;
    bool showNight() const;
    QVariantList profiles() const;
    QVariantList chartLayout() const;
    QVariantList chartCatalog() const;
    void setProfile(const QString &value);
    void setVisibleGroups(const QStringList &value);
    void setWindUnit(const QString &value);
    void setUtc(bool value);
    void setShowNight(bool value);
    Q_INVOKABLE void rememberPlace(const QString &name, double latitude, double longitude);
    Q_INVOKABLE void toggleFavorite(const QString &name, double latitude, double longitude);
    Q_INVOKABLE bool isFavorite(double latitude, double longitude) const;
    Q_INVOKABLE QString createProfile(const QString &name);
    Q_INVOKABLE void renameProfile(const QString &name);
    Q_INVOKABLE void deleteProfile();
    Q_INVOKABLE void setChartEnabled(const QString &key, bool enabled);
    Q_INVOKABLE void setChartField(const QString &chart, const QString &field, bool enabled);
    Q_INVOKABLE void moveChart(int index, int direction);
private:
    void saveLayout(const QVariantList &layout);
    QVariantList savedProfiles() const;
    QVariantList readPlaces(const QString &key) const;
    void writePlaces(const QString &key, const QVariantList &places);
    mutable QSettings m_settings;
signals:
    void placesChanged();
    void displayChanged();
};
