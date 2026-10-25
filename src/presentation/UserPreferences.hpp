#pragma once
#include <QObject>
#include <QSettings>
#include <QUrl>
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
    Q_PROPERTY(QVariantList seriesCatalog READ seriesCatalog CONSTANT)
    Q_PROPERTY(QString settingsFile READ settingsFile CONSTANT)
    Q_PROPERTY(QString renderer READ renderer WRITE setRenderer NOTIFY displayChanged)
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
    QVariantList seriesCatalog() const;
    QString settingsFile() const;
    QString renderer() const;
    void setRenderer(const QString &value);
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
    Q_INVOKABLE QString createChart(const QString &title);
    Q_INVOKABLE void deleteChart(const QString &key);
    Q_INVOKABLE void setChartOptions(const QString &key, const QVariantMap &options);
    Q_INVOKABLE void setSeriesStyle(const QString &chart, const QString &field, const QVariantMap &options);
    Q_INVOKABLE void removeSeries(const QString &chart, const QString &field);
    Q_INVOKABLE QVariantMap exportProfiles(const QUrl &file, bool allSaved = false) const;
    Q_INVOKABLE QVariantMap importProfiles(const QUrl &file);
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
