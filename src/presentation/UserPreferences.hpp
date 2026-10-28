#pragma once
#include "MeteoUiExport.hpp"
#include "PreferencesStore.hpp"
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT UserPreferences : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY placesChanged)
    Q_PROPERTY(QVariantList places READ favorites NOTIFY placesChanged)
    Q_PROPERTY(QVariantList recentPlaces READ recentPlaces NOTIFY placesChanged)
    Q_PROPERTY(QString profile READ profile WRITE setProfile NOTIFY displayChanged)
    Q_PROPERTY(QStringList visibleGroups READ visibleGroups WRITE setVisibleGroups NOTIFY displayChanged)
    Q_PROPERTY(QString windUnit READ windUnit WRITE setWindUnit NOTIFY displayChanged)
    Q_PROPERTY(bool utc READ utc WRITE setUtc NOTIFY displayChanged)
    Q_PROPERTY(bool showNight READ showNight WRITE setShowNight NOTIFY displayChanged)
    Q_PROPERTY(bool showLegend READ showLegend WRITE setShowLegend NOTIFY displayChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY displayChanged)
    Q_PROPERTY(bool hasDeletedProfiles READ hasDeletedProfiles NOTIFY displayChanged)
    Q_PROPERTY(QVariantList chartLayout READ chartLayout NOTIFY displayChanged)
    Q_PROPERTY(QVariantList chartCatalog READ chartCatalog CONSTANT)
    Q_PROPERTY(QVariantMap chartAppearance READ chartAppearance CONSTANT)
    Q_PROPERTY(QVariantList seriesCatalog READ seriesCatalog CONSTANT)
    Q_PROPERTY(QString settingsFile READ settingsFile CONSTANT)
    Q_PROPERTY(QString probeMode READ probeMode WRITE setProbeMode NOTIFY displayChanged)
    Q_PROPERTY(QString renderer READ renderer WRITE setRenderer NOTIFY displayChanged)
    Q_PROPERTY(int chartHeight READ chartHeight WRITE setChartHeight NOTIFY displayChanged)
    Q_PROPERTY(int profileChartHeight READ profileChartHeight WRITE setProfileChartHeight NOTIFY displayChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  public:
    explicit UserPreferences(QObject *parent = nullptr, QVariantList defaults = {});
    QString error() const { return m_error; }
    QVariantList favorites() const;
    QVariantList recentPlaces() const;
    QString profile() const;
    QStringList visibleGroups() const;
    QString windUnit() const;
    bool utc() const;
    bool showNight() const;
    bool showLegend() const;
    QVariantList profiles() const;
    bool hasDeletedProfiles() const;
    QVariantList chartLayout() const;
    Q_INVOKABLE QStringList seriesRenderers(const QString &key) const;
    Q_INVOKABLE QVariantList chartLayoutForProfile(const QString &id) const;
    QVariantList chartCatalog() const;
    QVariantMap chartAppearance() const;
    QVariantList seriesCatalog() const;
    QString settingsFile() const;
    QString probeMode() const;
    void setProbeMode(const QString &value);
    QString renderer() const;
    void setRenderer(const QString &value);
    int chartHeight() const;
    int profileChartHeight() const;
    Q_INVOKABLE int chartHeightForProfile(const QString &id) const;
    void setChartHeight(int value);
    void setProfileChartHeight(int value);
    void setProfile(const QString &value);
    void setVisibleGroups(const QStringList &value);
    void setWindUnit(const QString &value);
    void setUtc(bool value);
    void setShowNight(bool value);
    void setShowLegend(bool value);
    Q_INVOKABLE void savePlace(const QString &name, double latitude, double longitude);
    Q_INVOKABLE void removePlace(double latitude, double longitude);
    Q_INVOKABLE void rememberPlace(const QString &name, double latitude, double longitude);
    Q_INVOKABLE void toggleFavorite(const QString &name, double latitude, double longitude);
    Q_INVOKABLE bool isFavorite(double latitude, double longitude) const;
    Q_INVOKABLE QVariantMap placeAt(double latitude, double longitude) const;
    Q_INVOKABLE QVariantMap editPlace(const QString &id, const QString &name, double latitude, double longitude);
    Q_INVOKABLE QVariantMap deletePlace(const QString &id);
    Q_INVOKABLE QVariantMap restorePlace(const QVariantMap &place, int index);
    Q_INVOKABLE QString createProfile(const QString &name);
    Q_INVOKABLE QString duplicateProfile(const QString &profileId = {});
    Q_INVOKABLE void renameProfile(const QString &name, const QString &profileId = {});
    Q_INVOKABLE void deleteProfile(const QString &profileId = {});
    Q_INVOKABLE void resetProfile(const QString &profileId = {});
    Q_INVOKABLE void restoreDeletedProfiles();
    Q_INVOKABLE void updateProfile(const QString &id = {});
    Q_INVOKABLE void setChartEnabled(const QString &key, bool enabled, const QString &profileId = {});
    Q_INVOKABLE void setChartField(const QString &chart, const QString &field, bool enabled, const QString &profileId = {});
    Q_INVOKABLE void moveChart(int index, int direction, const QString &profileId = {});
    Q_INVOKABLE QString createChart(const QString &title, const QString &profileId = {});
    Q_INVOKABLE void addChart(const QString &key, const QString &profileId = {});
    Q_INVOKABLE void deleteChart(const QString &key, const QString &profileId = {});
    Q_INVOKABLE void setChartOptions(const QString &key, const QVariantMap &options, const QString &profileId = {});
    Q_INVOKABLE void setSeriesStyle(const QString &chart, const QString &field, const QVariantMap &options, const QString &profileId = {});
    Q_INVOKABLE void removeSeries(const QString &chart, const QString &field, const QString &profileId = {});
    Q_INVOKABLE QVariantMap exportProfiles(const QUrl &file, bool allSaved = false) const;
    Q_INVOKABLE QVariantMap exportProfilesByIds(const QUrl &file, const QStringList &ids) const;
    Q_INVOKABLE QVariantMap importProfiles(const QUrl &file, bool activate = true);
    Q_INVOKABLE QVariantMap profileState(const QString &id) const;
    Q_INVOKABLE QVariantMap restoreProfileState(const QVariantMap &before, const QVariantMap &after);
    Q_INVOKABLE void setProfileChartHeightForProfile(const QString &id, int height);

  private:
    bool change(const std::function<void()> &operation);
    void saveLayout(const QString &profile, const QVariantList &layout);
    void saveProfileOptions(const QString &profile, const QVariantMap &options);
    QVariantMap factoryProfile(const QString &id) const;
    QString createProfileFrom(const QString &name, const QString &sourceProfile, bool activate = true);
    QVariantList savedProfiles() const;
    QVariantList readPlaces(const QString &key) const;
    void writePlaces(const QString &key, const QVariantList &places);
    PreferencesStore m_store;
    QVariantList m_factoryProfiles;
    QString m_error;
  signals:
    void placesChanged();
    void displayChanged();
    void errorChanged();
};
