#include "core/Localization.hpp"
#include "WeatherModel.hpp"
#include "core/ForecastAggregation.hpp"
#include "ChartConfiguration.hpp"
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
        m_runs.append(QVariantMap{{"timestamp", 0}, {"label", Meteo::i18n("Latest forecast")}});
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
        m_runs.append(QVariantMap{{"timestamp", 0}, {"label", Meteo::i18n("Latest forecast")}});
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
        return Meteo::i18n("No forecast for the current hour");
    const auto &temperature = m_forecast->series[QStringLiteral("airtmp_point")];
    const auto timestamp = temperature.firstTimestamp
        + ((summaryTimestamp() - temperature.firstTimestamp) / temperature.interval) * temperature.interval;
    return Meteo::i18n("Forecast for ") + formatTime(timestamp, QStringLiteral("ddd HH:mm"));
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
    // Use the same zone as calendarDays/solarTimes, including Qt's fallback
    // when the operating system has no configured /etc/localtime.
    const auto zone = m_utc ? QTimeZone::utc() : QTimeZone::systemTimeZone();
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(qint64(timestamp), zone), pattern);
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
            text = code == 0 ? Meteo::i18n("None") : code == 1 ? Meteo::i18n("Rain")
                : code == 2 ? Meteo::i18n("Snow") : code == 3 ? Meteo::i18n("Sleet")
                : Meteo::i18n("Code %1").arg(code);
        }
        details.append(QVariantMap{{"key", field.key}, {"label", field.label}, {"value", text}, {"group", field.group},
                                   {"number", value ? QVariant(*value * scale) : QVariant()}, {"unit", unit}, {"color", field.color}, {"darkColor", forecastDarkColor(field.color, field.key)}});
    }
    return details;
}

void WeatherModel::loadRuns()
{
    m_runsRequest.get(QUrl(QStringLiteral("https://devmgramapi.meteo.pl/meteorograms/available")), 300);
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
    // This is the complete chart catalog, including optional standalone
    // direction arrows. Profiles decide which charts are actually displayed.
    auto catalog = defaultChartLayout("full");
    for (auto &value : catalog) { auto chart=value.toMap(); chart["enabled"]=true; value=chart; }
    return configuredCharts(catalog);
}

QVariantList WeatherModel::configuredCharts(const QVariantList &layout) const
{
    QVariantList result;
    for (const auto &entry : normalizeChartLayout(layout)) {
        auto chart = entry.toMap();
        if (!chart["enabled"].toBool()) continue;
        QVariantList fields;
        QString leftUnit, leftScale, rightScale;
        QStringList leftUnits, rightUnits;
        const auto series = chart["series"].toList();
        const auto unitFor = [this](const QVariantMap &style) { return style["unit"] == "m/s" ? m_windUnit : style["unit"].toString(); };
        for (const auto &value : series) {
            const auto style = value.toMap();
            if (style["enabled"].toBool() && style["axis"] != "right" && m_series.contains(style["key"].toString()) && style["render"] != "type") { leftUnit = unitFor(style); break; }
        }
        for (const auto &value : series) {
            auto style = value.toMap();
            const auto key = style["key"].toString();
            if (!style["enabled"].toBool() || !m_series.contains(key)) continue;
            const auto unit = unitFor(style);
            const auto axis = style["axis"] == "auto" ? (unit == leftUnit || style["render"] == "type" ? "left" : "right") : style["axis"].toString();
            style["axis"] = axis;
            style["unit"] = unit;
            style["kind"] = style["render"];
            style["points"] = m_series.value(key);
            if (style["render"] == "range" || style["render"] == "rangeStems") {
                const auto lower = m_series.value(style["lowerKey"].toString()).toList();
                if (lower.isEmpty()) style["kind"] = "line";
                else style["lowerPoints"] = lower;
            }
            fields.append(style);
            if (style["render"] == "type" || style["render"] == "direction" || style["render"] == "storm" || style["render"] == "flash") continue;
            auto &units = axis == "right" ? rightUnits : leftUnits;
            if (!unit.isEmpty() && !units.contains(unit)) units.append(unit);
            auto &scale = axis == "right" ? rightScale : leftScale;
            if (scale.isEmpty()) scale = seriesScale(key);
        }
        chart["fields"] = fields;
        chart["leftScale"] = chart["leftScale"] == "auto" ? (leftScale.isEmpty() ? "linear" : leftScale) : chart["leftScale"];
        chart["rightScale"] = chart["rightScale"] == "auto" ? (rightScale.isEmpty() ? "linked" : rightScale) : chart["rightScale"];
        chart["leftUnit"] = leftUnits.join(" / "); chart["rightUnit"] = rightUnits.join(" / ");
        if (!fields.isEmpty()) result.append(chart);
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
    if (!m_forecast) return result;
    const auto zone = m_utc ? QTimeZone::utc() : QTimeZone::systemTimeZone();
    const auto optional = [](const std::optional<double> &value) { return value ? QVariant(*value) : QVariant(); };
    for (const auto &aggregate : Meteo::aggregateDays(*m_forecast, zone)) {
        QVariantMap day{{"time", aggregate.date.startOfDay(zone).toSecsSinceEpoch()},
            {"day", QLocale().toString(aggregate.date, QStringLiteral("ddd d MMM"))},
            {"minimum", optional(aggregate.minimum)}, {"maximum", optional(aggregate.maximum)},
            {"rain", optional(aggregate.rain)}, {"wind", aggregate.gust ? QVariant(*aggregate.gust * displayScale("wind_gust_max",1)) : QVariant()},
            {"rainComplete", aggregate.rainComplete}, {"rainEstimated", aggregate.rainEstimatedAtBoundary},
            {"rainCoverageSeconds", aggregate.rainCoverageSeconds}, {"storm", aggregate.storm},
            {"snow", aggregate.snow}, {"mixed", aggregate.mixed},
            {"cloudCount", aggregate.cloudCover ? 1 : 0}, {"cloudSum", aggregate.cloudCover.value_or(0)*100}};
        QString icon, description;
        if (day["storm"].toBool()) { icon = "weather-storm"; description = Meteo::i18n("Possible thunderstorms"); }
        else if (day["snow"].toBool()) { icon = "weather-snow"; description = Meteo::i18n("Snowfall"); }
        else if (day["mixed"].toBool()) { icon = "weather-snow-rain"; description = Meteo::i18n("Sleet"); }
        else if (day["rain"].toDouble() > 0.1) { icon = "weather-showers"; description = Meteo::i18n("Rainfall"); }
        else if (!day["cloudCount"].toInt()) { icon = "weather-none-available"; description = Meteo::i18n("No cloud cover data"); }
        else {
            const auto clouds = day["cloudSum"].toDouble() / day["cloudCount"].toInt();
            icon = clouds > 80 ? "weather-overcast" : clouds > 25 ? "weather-clouds" : "weather-clear";
            description = clouds > 80 ? Meteo::i18n("Overcast") : clouds > 25 ? Meteo::i18n("Partly cloudy") : Meteo::i18n("Clear");
        }
        day["icon"] = icon; day["description"] = description;
        for (const auto &key : {"cloudSum", "cloudCount", "storm", "snow", "mixed"}) day.remove(key);
        result.append(day);
    }
    return result;
}

QVariantList WeatherModel::calendarDays(double from, double to) const
{
    QVariantList result;
    if (!std::isfinite(from) || !std::isfinite(to) || to <= from || to - from > 366 * 86400.0) return result;
    const auto zone = m_utc ? QTimeZone::utc() : QTimeZone::systemTimeZone();
    auto date = QDateTime::fromSecsSinceEpoch(qint64(from), zone).date();
    const auto last = QDateTime::fromSecsSinceEpoch(qint64(to) - 1, zone).date();
    for (; date <= last; date = date.addDays(1)) {
        result.append(QVariantMap{{"start", date.startOfDay(zone).toSecsSinceEpoch()},
            {"end", date.addDays(1).startOfDay(zone).toSecsSinceEpoch()}});
    }
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

QVariantMap WeatherModel::solarTimes() const
{
    if (!m_forecast) return {};
    const auto zone = m_utc ? QTimeZone::utc() : QTimeZone::systemTimeZone();
    const auto timestamp = summaryTimestamp();
    const auto day = QDateTime::fromSecsSinceEpoch(timestamp ? timestamp : m_forecast->firstTimestamp(), zone).date();
    const auto start = day.startOfDay(zone).toSecsSinceEpoch();
    const auto end = day.addDays(1).startOfDay(zone).toSecsSinceEpoch();
    bool previous = nightAt(start, m_latitude, m_longitude);
    qint64 sunrise = 0, sunset = 0;
    for (auto time = start + 60; time < end; time += 60) {
        const auto night = nightAt(time, m_latitude, m_longitude);
        if (previous && !night) sunrise = time;
        if (!previous && night) sunset = time;
        previous = night;
    }
    return {{"date", day.toString(Qt::ISODate)}, {"dateLabel", QLocale().toString(day, QStringLiteral("d MMM"))},
        {"today", day == QDateTime::currentDateTimeUtc().toTimeZone(zone).date()},
        {"sunrise", sunrise}, {"sunset", sunset},
        {"sunriseText", sunrise ? formatTime(sunrise, "HH:mm") : QStringLiteral("—")},
        {"sunsetText", sunset ? formatTime(sunset, "HH:mm") : QStringLiteral("—")},
        {"status", !sunrise && !sunset ? (previous ? Meteo::i18n("Polar night") : Meteo::i18n("Polar day")) : QString()}};
}

QString WeatherModel::sunTimes() const
{
    const auto solar = solarTimes();
    if (solar.isEmpty()) return {};
    if (!solar["status"].toString().isEmpty()) return solar["status"].toString();
    return Meteo::i18n("%1 · sunrise ≈ %2 · sunset ≈ %3")
        .arg(solar["dateLabel"].toString(), solar["sunriseText"].toString(), solar["sunsetText"].toString());
}
