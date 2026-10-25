#pragma once
#include "MeteoUiExport.hpp"

#include "core/Forecast.hpp"
#include "network/MeteoClient.hpp"
#include "network/JsonRequest.hpp"
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT WeatherModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(quint64 dataRevision READ dataRevision NOTIFY dataChanged)
    Q_PROPERTY(double latitude READ latitude WRITE setLatitude NOTIFY locationChanged)
    Q_PROPERTY(double longitude READ longitude WRITE setLongitude NOTIFY locationChanged)
    Q_PROPERTY(QString locationName READ locationName WRITE setLocationName NOTIFY locationChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY locationChanged)
    Q_PROPERTY(bool autoRefresh READ autoRefresh WRITE setAutoRefresh NOTIFY autoRefreshChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY dataChanged)
    Q_PROPERTY(bool usingCache READ usingCache NOTIFY stateChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString modelName READ modelName NOTIFY locationChanged)
    Q_PROPERTY(QString runText READ runText NOTIFY dataChanged)
    Q_PROPERTY(QString updatedText READ updatedText NOTIFY stateChanged)
    Q_PROPERTY(QString temperatureText READ temperatureText NOTIFY stateChanged)
    Q_PROPERTY(QString windText READ windText NOTIFY stateChanged)
    Q_PROPERTY(QString pressureText READ pressureText NOTIFY stateChanged)
    Q_PROPERTY(QString summaryTime READ summaryTime NOTIFY stateChanged)
    Q_PROPERTY(QString weatherIcon READ weatherIcon NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap series READ series NOTIFY dataChanged)
    Q_PROPERTY(double firstTimestamp READ firstTimestamp NOTIFY dataChanged)
    Q_PROPERTY(double lastTimestamp READ lastTimestamp NOTIFY dataChanged)
    Q_PROPERTY(QVariantList chartGroups READ chartGroups NOTIFY dataChanged)
    Q_PROPERTY(QVariantList dailyForecast READ dailyForecast NOTIFY dataChanged)
    Q_PROPERTY(QVariantList hourlyForecast READ hourlyForecast NOTIFY dataChanged)
    Q_PROPERTY(QVariantList nightIntervals READ nightIntervals NOTIFY dataChanged)
    Q_PROPERTY(QVariantList availableRuns READ availableRuns NOTIFY runsChanged)
    Q_PROPERTY(double requestedRun READ requestedRun WRITE setRequestedRun NOTIFY runsChanged)
    Q_PROPERTY(bool archive READ archive NOTIFY stateChanged)
    Q_PROPERTY(QString windUnit READ windUnit WRITE setWindUnit NOTIFY dataChanged)
    Q_PROPERTY(bool utc READ utc WRITE setUtc NOTIFY dataChanged)
    Q_PROPERTY(QUrl originalLookupUrl READ originalLookupUrl NOTIFY dataChanged)
    Q_PROPERTY(QString sunTimes READ sunTimes NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap solarTimes READ solarTimes NOTIFY stateChanged)
public:
    explicit WeatherModel(QObject *parent = nullptr);
    quint64 dataRevision() const { return m_dataRevision; }
    double latitude() const { return m_latitude; }
    double longitude() const { return m_longitude; }
    QString locationName() const { return m_locationName; }
    QString model() const { return m_model; }
    bool autoRefresh() const { return m_autoRefresh; }
    bool busy() const { return m_busy; }
    bool hasData() const { return m_forecast.has_value(); }
    bool usingCache() const { return m_usingCache; }
    bool stale() const;
    QString error() const { return m_error; }
    QString modelName() const { return Meteo::modelLabel(m_model); }
    QString runText() const;
    QString updatedText() const;
    QString temperatureText() const;
    QString windText() const;
    QString pressureText() const;
    QString summaryTime() const;
    QString weatherIcon() const;
    QVariantMap series() const { return m_series; }
    double firstTimestamp() const { return m_forecast ? m_forecast->firstTimestamp() : 0; }
    double lastTimestamp() const { return m_forecast ? m_forecast->lastTimestamp() : 0; }
    QVariantList chartGroups() const;
    QVariantList dailyForecast() const;
    QVariantList hourlyForecast() const;
    QVariantList nightIntervals() const;
    QVariantList availableRuns() const { return m_runs; }
    double requestedRun() const { return m_requestedRun; }
    bool archive() const { return m_requestedRun > 0; }
    QString windUnit() const { return m_windUnit; }
    bool utc() const { return m_utc; }
    QUrl originalLookupUrl() const;
    QString sunTimes() const;
    QVariantMap solarTimes() const;
    void setRequestedRun(double value);
    void setWindUnit(const QString &value);
    void setUtc(bool value);
    Q_INVOKABLE QVariantList detailsAt(double timestamp) const;
    Q_INVOKABLE QVariantList configuredCharts(const QVariantList &layout) const;
    Q_INVOKABLE QVariantList calendarDays(double from, double to) const;
    Q_INVOKABLE double nearestTimestamp(double timestamp) const;
    Q_INVOKABLE QString formatTime(double timestamp, const QString &pattern = QStringLiteral("ddd d MMM, HH:mm")) const;
    Q_INVOKABLE void loadRuns();
    void setLatitude(double value);
    void setLongitude(double value);
    void setLocationName(const QString &value);
    void setModel(const QString &value);
    void setAutoRefresh(bool value);
    Q_INVOKABLE void refresh(bool force = true);

signals:
    void locationChanged();
    void autoRefreshChanged();
    void stateChanged();
    void dataChanged();
    void runsChanged();

private slots:
    void updateCurrentTime() { emit stateChanged(); }
private:
    void resetLocation();
    void apply(const Meteo::Forecast &forecast);
    QString cachePath() const;
    bool loadCache();
    void saveCache(const QByteArray &json) const;
    qint64 summaryTimestamp() const;
    QString valueText(const QString &key, const QString &suffix, double scale = 1) const;
    double displayScale(const QString &key, double baseScale) const;

    Meteo::MeteoClient m_client;
    Meteo::JsonRequest m_runsRequest;
    QTimer m_debounce;
    QTimer m_refreshTimer;
    QTimer m_clockTimer;
    double m_latitude = 52.2297;
    double m_longitude = 21.0122;
    QString m_locationName = QStringLiteral("Warszawa");
    QString m_model = QStringLiteral("um4_60");
    bool m_autoRefresh = true;
    bool m_busy = false;
    bool m_usingCache = false;
    QString m_error;
    QDateTime m_updated;
    std::optional<Meteo::Forecast> m_forecast;
    QVariantMap m_series;
    quint64 m_dataRevision = 0;
    QVariantList m_runs;
    qint64 m_requestedRun = 0;
    QString m_windUnit = QStringLiteral("m/s");
    bool m_utc = false;
};
