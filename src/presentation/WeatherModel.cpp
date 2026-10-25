#include "WeatherModel.hpp"
#include "ForecastFields.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>
#include <QTimeZone>
#include <numbers>
#include <QUrlQuery>

namespace {
bool nightAt(qint64 timestamp, double latitude, double longitude)
{
    constexpr double rad = std::numbers::pi / 180.0;
    const auto time = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::UTC);
    const double gamma = 2 * std::numbers::pi / 365 * (time.date().dayOfYear() - 1 + (time.time().hour() - 12.0) / 24);
    const double declination = 0.006918 - 0.399912 * std::cos(gamma) + 0.070257 * std::sin(gamma)
        - 0.006758 * std::cos(2 * gamma) + 0.000907 * std::sin(2 * gamma) - 0.002697 * std::cos(3 * gamma) + 0.00148 * std::sin(3 * gamma);
    const double equation = 229.18 * (0.000075 + 0.001868 * std::cos(gamma) - 0.032077 * std::sin(gamma)
        - 0.014615 * std::cos(2 * gamma) - 0.040849 * std::sin(2 * gamma));
    const double minutes = time.time().hour() * 60 + time.time().minute() + equation + 4 * longitude;
    const double hourAngle = (minutes / 4 - 180) * rad;
    const double elevation = std::asin(std::sin(latitude * rad) * std::sin(declination)
        + std::cos(latitude * rad) * std::cos(declination) * std::cos(hourAngle)) / rad;
    return elevation < -0.833;
}
}

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
    connect(&m_client, &Meteo::MeteoClient::runsReceived, this, [this](const QList<qint64> &runs) {
        m_runs.clear();
        m_runs.append(QVariantMap{{"timestamp", 0}, {"label", QStringLiteral("Najnowsza prognoza")}});
        for (const auto run : runs)
            m_runs.append(QVariantMap{{"timestamp", run}, {"label", formatTime(run, QStringLiteral("ddd d MMM · HH:mm t"))}});
        emit runsChanged();
    });
    connect(&m_runsRequest, &Meteo::JsonRequest::received, this, [this](const QJsonDocument &document) {
        QList<qint64> runs;
        for (const auto &value : document.object().value(m_model).toArray()) {
            const auto run = value.isString() ? value.toString().toLongLong() : value.toInteger();
            if (run > 0 && !runs.contains(run)) runs.append(run);
        }
        std::sort(runs.begin(), runs.end(), std::greater<>());
        m_runs.clear();
        m_runs.append(QVariantMap{{"timestamp", 0}, {"label", QStringLiteral("Najnowsza prognoza")}});
        for (const auto run : runs)
            m_runs.append(QVariantMap{{"timestamp", run}, {"label", formatTime(run, QStringLiteral("ddd d MMM · HH:mm t"))}});
        emit runsChanged();
    });
    connect(&m_runsRequest, &Meteo::JsonRequest::failed, this, [this](const QString &error) { m_error = error; emit stateChanged(); });
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
    m_requestedRun = 0;
    m_runs.clear();
    m_runsRequest.cancel();
    emit runsChanged();
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
    m_client.fetch(m_latitude, m_longitude, m_model, m_requestedRun);
}

void WeatherModel::apply(const Meteo::Forecast &forecast)
{
    m_forecast = forecast;
    m_series.clear();
    for (auto it = forecast.series.cbegin(); it != forecast.series.cend(); ++it) {
        QVariantList points;
        const auto &series = it.value();
        double scale = 1;
        for (const auto &field : forecastFields())
            if (field.key == it.key()) { scale = displayScale(field.key, field.scale); break; }
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
        + QStringLiteral("/meteo-pl/") + QString::fromLatin1(hash)
        + (m_requestedRun > 0 ? QStringLiteral("-%1").arg(m_requestedRun) : QString()) + QStringLiteral(".json");
}

bool WeatherModel::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 3 * 1024 * 1024)
        return false;
    const auto envelope = QJsonDocument::fromJson(file.readAll()).object();
    if (envelope.value("model").toString() != m_model || envelope.value("latitude").toDouble() != m_latitude
        || envelope.value("longitude").toDouble() != m_longitude
        || envelope.value("requestedRun").toInteger() != m_requestedRun)
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
    const QJsonObject envelope{{"model", m_model}, {"latitude", m_latitude}, {"longitude", m_longitude}, {"requestedRun", m_requestedRun},
                               {"savedAt", m_updated.toString(Qt::ISODate)}, {"payload", QString::fromUtf8(json)}};
    file.write(QJsonDocument(envelope).toJson(QJsonDocument::Compact));
    file.commit();
}

bool WeatherModel::stale() const
{
    return !archive() && hasData() && (m_updated.secsTo(QDateTime::currentDateTimeUtc()) > 6 * 3600
                         || m_forecast->lastTimestamp() < QDateTime::currentSecsSinceEpoch());
}

QString WeatherModel::runText() const
{
    return hasData() ? formatTime(m_forecast->modelRun.toSecsSinceEpoch(), QStringLiteral("ddd, d MMM · HH:mm t")) : QString();
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
QString WeatherModel::windText() const { return valueText("wind10_sd_true_prev_point", ' ' + m_windUnit, displayScale("wind10_sd_true_prev_point", 1)); }
QString WeatherModel::pressureText() const { return valueText("slpres_point", QStringLiteral(" hPa"), 0.01); }

QString WeatherModel::summaryTime() const
{
    if (!summaryTimestamp())
        return QStringLiteral("Brak prognozy na bieżącą godzinę");
    const auto &temperature = m_forecast->series[QStringLiteral("airtmp_point")];
    const auto timestamp = temperature.firstTimestamp
        + ((summaryTimestamp() - temperature.firstTimestamp) / temperature.interval) * temperature.interval;
    return QStringLiteral("Prognoza na ") + formatTime(timestamp, QStringLiteral("ddd HH:mm"));
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

void WeatherModel::setRequestedRun(double value)
{
    const auto run = std::isfinite(value) && value > 0 ? qint64(value) : 0;
    if (m_requestedRun == run) return;
    m_requestedRun = run;
    resetLocation();
    emit runsChanged();
}

void WeatherModel::setWindUnit(const QString &value)
{
    if (m_windUnit == value || !QStringList{"m/s", "km/h", "kt"}.contains(value)) return;
    m_windUnit = value;
    if (m_forecast) apply(*m_forecast);
    emit stateChanged();
}

void WeatherModel::setUtc(bool value)
{
    if (m_utc == value) return;
    m_utc = value;
    emit stateChanged();
}

double WeatherModel::displayScale(const QString &key, double baseScale) const
{
    if (key.startsWith("cldbse") || key == "cldtop") {
        const auto unit = m_forecast ? m_forecast->series.value(key).unit.trimmed().toLower() : QString();
        return unit == "m" ? 0.001 : 1;
    }
    if (key == "wind10_sd_true_prev_point" || key == "wind_gust_max")
        return m_windUnit == "km/h" ? 3.6 : m_windUnit == "kt" ? 1.943844492 : 1;
    return baseScale;
}

QString WeatherModel::formatTime(double timestamp, const QString &pattern) const
{
    const auto time = QDateTime::fromSecsSinceEpoch(qint64(timestamp));
    return QLocale().toString(m_utc ? time.toUTC() : time.toLocalTime(), pattern);
}

double WeatherModel::nearestTimestamp(double timestamp) const
{
    if (!m_forecast || !std::isfinite(timestamp)) return 0;
    const auto &series = m_forecast->series[QStringLiteral("airtmp_point")];
    const auto index = std::clamp(qRound64((timestamp - series.firstTimestamp) / series.interval), qint64(0), qint64(series.values.size() - 1));
    return series.firstTimestamp + index * series.interval;
}

QVariantList WeatherModel::detailsAt(double timestamp) const
{
    QVariantList details;
    if (!m_forecast || !std::isfinite(timestamp)) return details;
    const auto time = qint64(nearestTimestamp(timestamp));
    const QStringList directions{"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
    for (const auto &field : forecastFields()) {
        const auto it = m_forecast->series.constFind(field.key);
        if (it == m_forecast->series.cend()) continue;
        const auto value = it->at(time);
        const auto scale = displayScale(field.key, field.scale);
        const auto unit = field.unit == "m/s" ? m_windUnit : field.unit;
        QString text = value ? QLocale().toString(*value * scale, 'f', field.decimals) + (unit.isEmpty() ? QString() : ' ' + unit) : QStringLiteral("—");
        if (field.kind == "direction" && value) text += " · " + directions[(qRound(*value / 22.5) % 16 + 16) % 16];
        if (field.kind == "type" && value) {
            // Precipitation categories used by the upstream client.
            const int code = qRound(*value);
            text = code == 0 ? QStringLiteral("Brak") : code == 1 ? QStringLiteral("Deszcz")
                : code == 2 ? QStringLiteral("Śnieg") : code == 3 ? QStringLiteral("Deszcz ze śniegiem")
                : QStringLiteral("Kod %1").arg(code);
        }
        details.append(QVariantMap{{"key", field.key}, {"label", field.label}, {"value", text}, {"group", field.group},
                                   {"number", value ? QVariant(*value * scale) : QVariant()}, {"color", field.color}, {"darkColor", forecastDarkColor(field.color)}});
    }
    return details;
}

void WeatherModel::loadRuns()
{
    m_runsRequest.get(QUrl(QStringLiteral("https://devmgramapi.meteo.pl/meteorograms/available")));
}

QUrl WeatherModel::originalLookupUrl() const
{
    if (!m_forecast || m_model == "gfs") return {};
    QUrl url(QStringLiteral("https://old.meteo.pl/um/php/mgram_search.php"));
    QUrlQuery query;
    query.addQueryItem("NALL", QString::number(m_latitude, 'f', 6));
    query.addQueryItem("EALL", QString::number(m_longitude, 'f', 6));
    query.addQueryItem("fdate", m_forecast->modelRun.toUTC().toString(QStringLiteral("yyyyMMddhh")));
    query.addQueryItem("lang", "pl"); url.setQuery(query); return url;
}

QVariantList WeatherModel::chartGroups() const
{
    QVariantList result;
    for (const auto &group : forecastCharts()) {
        QVariantList fields;
        for (const auto &key : group.fields) for (const auto &field : forecastFields()) {
            if (field.key != key || !m_series.contains(key)) continue;
            auto kind = field.kind;
            if (group.key == "temperature" || group.key == "ground") {
                if (key == "grdtmp_min") kind = "groundLow";
                if (key == "grdtmp_max") kind = "groundHigh";
                if (key == "wchill_min") kind = "feelLower";
                if (key == "wchill_max") kind = "feelUpper";
            }
            if (group.key == "precipitation" && key == "realhum_aver") kind = "right";
            if (group.key == "probability") kind = key == "pcpttlprob_point" ? "line" : key == "storm_max" ? "storm" : "flash";
            if (group.key == "wind" && key == "wind_gust_max") kind = "gusts";
            if (group.key == "cloudHeights" && key == "visibl_min") kind = "right";
            else if (group.key == "cloudHeights") kind = "cloudPoint";
            if (group.key == "clouds") {
                if (key == "fog_max") kind = "rightBars";
                else if (key != "cldtot_aver") kind = "cloudArea";
            }
            fields.append(QVariantMap{{"key", field.key}, {"label", field.label}, {"unit", field.unit == "m/s" ? m_windUnit : field.unit},
                                      {"color", field.color}, {"darkColor", forecastDarkColor(field.color)}, {"kind", kind}, {"points", m_series.value(field.key)}});
        }
        if (!fields.isEmpty()) result.append(QVariantMap{{"key", group.key}, {"title", group.title}, {"fields", fields}});
    }
    return result;
}

QVariantList WeatherModel::hourlyForecast() const
{
    QVariantList rows;
    if (!m_forecast) return rows;
    const auto &base = m_forecast->series[QStringLiteral("airtmp_point")];
    for (qsizetype i = 0; i < base.values.size(); ++i) {
        const auto time = base.firstTimestamp + i * base.interval;
        QVariantMap values{{"time", time}, {"label", formatTime(time, QStringLiteral("ddd HH:mm"))}};
        for (const auto &field : forecastFields()) {
            const auto value = m_forecast->series.value(field.key).at(time);
            values.insert(field.key, value ? QVariant(*value * displayScale(field.key, field.scale)) : QVariant());
        }
        rows.append(values);
    }
    return rows;
}

QVariantList WeatherModel::dailyForecast() const
{
    QVariantList result;
    const auto hours = hourlyForecast();
    QMap<QDate, QVariantMap> days;
    for (const auto &entry : hours) {
        const auto row = entry.toMap();
        const auto timestamp = row["time"].toLongLong();
        auto time = QDateTime::fromSecsSinceEpoch(timestamp);
        if (m_utc) time = time.toUTC();
        const auto date = time.date();
        if (!days.contains(date)) days.insert(date, {{"time", timestamp}, {"day", QLocale().toString(date, QStringLiteral("ddd d MMM"))},
                                                    {"minimum", QVariant()}, {"maximum", QVariant()}, {"rain", QVariant()}, {"wind", QVariant()}});
        auto &day = days[date];
        const auto low = row.value("airtmp_min").isNull() ? row.value("airtmp_point") : row.value("airtmp_min");
        const auto high = row.value("airtmp_max").isNull() ? row.value("airtmp_point") : row.value("airtmp_max");
        if (!low.isNull()) day["minimum"] = day["minimum"].isNull() ? low.toDouble() : std::min(day["minimum"].toDouble(), low.toDouble());
        if (!high.isNull()) day["maximum"] = day["maximum"].isNull() ? high.toDouble() : std::max(day["maximum"].toDouble(), high.toDouble());
        if (!row.value("pcpttl_aver").isNull()) day["rain"] = day["rain"].toDouble() + row["pcpttl_aver"].toDouble();
        if (!row.value("wind_gust_max").isNull()) day["wind"] = day["wind"].isNull() ? row["wind_gust_max"].toDouble() : std::max(day["wind"].toDouble(), row["wind_gust_max"].toDouble());
    }
    for (const auto &day : days) result.append(day);
    return result;
}

QVariantList WeatherModel::nightIntervals() const
{
    QVariantList result;
    if (!m_forecast) return result;
    qint64 start = 0;
    const auto end = m_forecast->lastTimestamp();
    for (auto time = m_forecast->firstTimestamp(); time <= end; time += 900) {
        const bool night = nightAt(time, m_latitude, m_longitude);
        if (night && !start) start = time;
        if (!night && start) { result.append(QVariantMap{{"start", start}, {"end", time}}); start = 0; }
    }
    if (start) result.append(QVariantMap{{"start", start}, {"end", end}});
    return result;
}

QString WeatherModel::sunTimes() const
{
    if (!m_forecast) return {};
    const auto zone = m_utc ? QTimeZone::utc() : QTimeZone::systemTimeZone();
    const auto day = QDateTime::fromSecsSinceEpoch(m_forecast->firstTimestamp(), zone).date();
    const auto start = day.startOfDay(zone).toSecsSinceEpoch();
    const auto end = day.addDays(1).startOfDay(zone).toSecsSinceEpoch();
    bool previous = nightAt(start, m_latitude, m_longitude);
    qint64 sunrise = 0, sunset = 0;
    for (auto time = start + 60; time <= end; time += 60) {
        const auto night = nightAt(time, m_latitude, m_longitude);
        if (previous && !night) sunrise = time;
        if (!previous && night) sunset = time;
        previous = night;
    }
    if (!sunrise && !sunset) return previous ? QStringLiteral("Noc polarna") : QStringLiteral("Dzień polarny");
    return QStringLiteral("%1 · wschód ≈ %2 · zachód ≈ %3")
        .arg(QLocale().toString(day, QStringLiteral("d MMM")), sunrise ? formatTime(sunrise, "HH:mm") : QStringLiteral("—"), sunset ? formatTime(sunset, "HH:mm t") : QStringLiteral("—"));
}
