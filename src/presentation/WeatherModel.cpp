#include "WeatherModel.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>

WeatherModel::WeatherModel(QObject *parent) : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(0);
    connect(&m_debounce, &QTimer::timeout, this, [this] { refresh(false); });
    m_refreshTimer.setInterval(60 * 60 * 1000);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this] { refresh(false); });
    m_refreshTimer.start();
    m_clockTimer.setInterval(60000);
    connect(&m_clockTimer, &QTimer::timeout, this, &WeatherModel::stateChanged);
    m_clockTimer.start();
    connect(&m_client, &Meteo::MeteoClient::received, this, [this](const QByteArray &json) {
        QString error;
        const auto forecast = Meteo::parseForecast(json, &error);
        m_busy = false;
        if (forecast) {
            m_updated = QDateTime::currentDateTimeUtc();
            m_usingCache = false;
            m_error.clear();
            apply(*forecast);
            saveCache(json);
        } else {
            m_error = error;
        }
        emit stateChanged();
    });
    connect(&m_client, &Meteo::MeteoClient::failed, this, [this](const QString &error) {
        m_busy = false;
        m_error = error;
        m_usingCache = hasData();
        emit stateChanged();
    });
    m_debounce.start();
}

void WeatherModel::setLatitude(double value)
{
    if (m_latitude == value)
        return;
    m_latitude = value;
    resetLocation();
}

void WeatherModel::setLongitude(double value)
{
    if (m_longitude == value)
        return;
    m_longitude = value;
    resetLocation();
}

void WeatherModel::setLocationName(const QString &value)
{
    if (m_locationName == value)
        return;
    m_locationName = value;
    emit locationChanged();
}

void WeatherModel::setModel(const QString &value)
{
    if (m_model == value)
        return;
    m_model = value;
    resetLocation();
}

void WeatherModel::setAutoRefresh(bool value)
{
    if (m_autoRefresh == value)
        return;
    m_autoRefresh = value;
    if (value) {
        m_refreshTimer.start();
        m_debounce.start();
    } else {
        m_refreshTimer.stop();
        m_debounce.stop();
        m_client.cancel();
        m_busy = false;
        emit stateChanged();
    }
    emit autoRefreshChanged();
}

void WeatherModel::resetLocation()
{
    m_client.cancel();
    m_busy = false;
    m_forecast.reset();
    m_series.clear();
    m_error.clear();
    m_updated = {};
    m_usingCache = false;
    emit locationChanged();
    emit stateChanged();
    if (m_autoRefresh)
        m_debounce.start();
}

void WeatherModel::refresh(bool force)
{
    if (m_busy)
        return;
    if (!hasData())
        loadCache();
    // Multiple views can reuse a recent cache without fetching the same forecast.
    if (!force && m_error.isEmpty() && m_updated.isValid()
        && m_updated.secsTo(QDateTime::currentDateTimeUtc()) < 3600 && !stale()) {
        emit stateChanged();
        return;
    }
    m_busy = true;
    m_error.clear();
    emit stateChanged();
    m_client.fetch(m_latitude, m_longitude, m_model);
}

void WeatherModel::apply(const Meteo::Forecast &forecast)
{
    m_forecast = forecast;
    m_series.clear();
    for (auto it = forecast.series.cbegin(); it != forecast.series.cend(); ++it) {
        QVariantList points;
        const auto &series = it.value();
        double scale = 1;
        if (it.key() == "slpres_point")
            scale = 0.01;
        if (it.key() == "cldtot_aver" || it.key() == "cldvlow_aver" || it.key() == "cldlow_aver"
            || it.key() == "cldmed_aver" || it.key() == "cldhigh_aver" || it.key() == "fog_max")
            scale = 100;
        for (qsizetype i = 0; i < series.values.size(); ++i) {
            const auto &value = series.values.at(i);
            points.append(QVariantMap{{"time", series.firstTimestamp + qint64(i) * series.interval},
                                      {"value", value ? QVariant(*value * scale) : QVariant()}});
        }
        m_series.insert(it.key(), points);
    }
}

QString WeatherModel::cachePath() const
{
    const auto identity = m_model + ':' + QString::number(m_latitude, 'f', 6) + ':' + QString::number(m_longitude, 'f', 6);
    const auto hash = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/meteo-pl/") + QString::fromLatin1(hash) + QStringLiteral(".json");
}

bool WeatherModel::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 3 * 1024 * 1024)
        return false;
    const auto envelope = QJsonDocument::fromJson(file.readAll()).object();
    if (envelope.value("model").toString() != m_model || envelope.value("latitude").toDouble() != m_latitude
        || envelope.value("longitude").toDouble() != m_longitude)
        return false;
    const auto updated = QDateTime::fromString(envelope.value("savedAt").toString(), Qt::ISODate);
    if (!updated.isValid() || updated > QDateTime::currentDateTimeUtc())
        return false;
    const auto parsed = Meteo::parseForecast(envelope.value("payload").toString().toUtf8());
    if (!parsed)
        return false;
    apply(*parsed);
    m_updated = updated;
    m_usingCache = true;
    return true;
}

void WeatherModel::saveCache(const QByteArray &json) const
{
    const auto path = cachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    const QJsonObject envelope{{"model", m_model}, {"latitude", m_latitude}, {"longitude", m_longitude},
                               {"savedAt", m_updated.toString(Qt::ISODate)}, {"payload", QString::fromUtf8(json)}};
    file.write(QJsonDocument(envelope).toJson(QJsonDocument::Compact));
    file.commit();
}

bool WeatherModel::stale() const
{
    return hasData() && (m_updated.secsTo(QDateTime::currentDateTimeUtc()) > 6 * 3600
                         || m_forecast->lastTimestamp() < QDateTime::currentSecsSinceEpoch());
}

QString WeatherModel::runText() const
{
    return hasData() ? QLocale().toString(m_forecast->modelRun.toLocalTime(), QStringLiteral("ddd, d MMM · HH:mm t")) : QString();
}

QString WeatherModel::updatedText() const
{
    return m_updated.isValid() ? QLocale().toString(m_updated.toLocalTime(), QStringLiteral("d MMM, HH:mm")) : QString();
}

qint64 WeatherModel::summaryTimestamp() const
{
    if (!hasData())
        return 0;
    const auto now = QDateTime::currentSecsSinceEpoch();
    if (now > m_forecast->lastTimestamp())
        return 0;
    return std::max(now, m_forecast->firstTimestamp());
}

QString WeatherModel::valueText(const QString &key, const QString &suffix, double scale) const
{
    if (!summaryTimestamp())
        return QStringLiteral("—");
    const auto it = m_forecast->series.constFind(key);
    if (it == m_forecast->series.cend())
        return QStringLiteral("—");
    const auto value = it->at(summaryTimestamp());
    return value ? QLocale().toString(*value * scale, 'f', 0) + suffix : QStringLiteral("—");
}

QString WeatherModel::temperatureText() const { return valueText("airtmp_point", QStringLiteral("°C")); }
QString WeatherModel::windText() const { return valueText("wind10_sd_true_prev_point", QStringLiteral(" m/s")); }
QString WeatherModel::pressureText() const { return valueText("slpres_point", QStringLiteral(" hPa"), 0.01); }

QString WeatherModel::summaryTime() const
{
    if (!summaryTimestamp())
        return QStringLiteral("Brak prognozy na bieżącą godzinę");
    const auto &temperature = m_forecast->series[QStringLiteral("airtmp_point")];
    const auto timestamp = temperature.firstTimestamp
        + ((summaryTimestamp() - temperature.firstTimestamp) / temperature.interval) * temperature.interval;
    return QStringLiteral("Prognoza na ") + QLocale().toString(QDateTime::fromSecsSinceEpoch(timestamp), QStringLiteral("ddd HH:mm"));
}

QString WeatherModel::weatherIcon() const
{
    if (!summaryTimestamp())
        return QStringLiteral("weather-none-available");
    const auto rain = m_forecast->series.value("pcpttl_aver").at(summaryTimestamp());
    if (rain && *rain > 0.1)
        return QStringLiteral("weather-showers");
    const auto clouds = m_forecast->series.value("cldtot_aver").at(summaryTimestamp());
    if (!clouds)
        return QStringLiteral("weather-none-available");
    if (*clouds > 0.8)
        return QStringLiteral("weather-overcast");
    if (*clouds > 0.25)
        return QStringLiteral("weather-clouds");
    const auto hour = QDateTime::fromSecsSinceEpoch(summaryTimestamp()).time().hour();
    return hour < 6 || hour >= 19 ? QStringLiteral("weather-clear-night") : QStringLiteral("weather-clear");
}
