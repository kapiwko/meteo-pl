#include "core/Localization.hpp"
#include "WeatherModel.hpp"
#include "core/ForecastAggregation.hpp"
#include "core/ForecastSampling.hpp"
#include "ChartDataBuilder.hpp"
#include "ChartConfiguration.hpp"
#include "ForecastFields.hpp"

#include <QLocale>
#include <cmath>
#include <QTimeZone>
#include "core/SolarCalendar.hpp"
#include <QUrlQuery>


WeatherModel::WeatherModel(QObject *parent, QUrl endpoint) : QObject(parent), m_repository(nullptr,std::move(endpoint))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(0);
    connect(&m_debounce, &QTimer::timeout, this, [this] { refresh(false); });
    m_refreshTimer.setInterval(60 * 60 * 1000);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this] { refresh(false); });
    m_refreshTimer.start();
    m_clockTimer.setInterval(60000);
    connect(&m_clockTimer, &QTimer::timeout, this, &WeatherModel::updateCurrentTime);
    m_clockTimer.start();
    connect(&m_repository, &Meteo::ForecastRepository::runsReceived, this, [this](const QList<qint64> &runs) {
        m_runs = {0};
        m_runs.append(runs);
        emit runsChanged();
    });
    connect(&m_repository, &Meteo::ForecastRepository::catalogFailed, this, [this](const QString &error) { m_error=error; emit stateChanged(); });
    connect(&m_repository, &Meteo::ForecastRepository::received, this, [this](const Meteo::ForecastSnapshot &snapshot) {
        m_busy=false;
        m_updated=snapshot.fetchedAt;
        m_usingCache=snapshot.usingCache;
        m_error.clear();
        apply(snapshot.forecast);
        emit stateChanged();
    });
    connect(&m_repository, &Meteo::ForecastRepository::failed, this, [this](const QString &error) {
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
    m_repository.cancel();
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
        m_repository.cancel();
        m_busy = false;
        emit stateChanged();
    }
    emit autoRefreshChanged();
}

void WeatherModel::resetLocation()
{
    m_repository.cancel();
    m_busy = false;
    m_forecast.reset();
    m_series.clear();
    m_error.clear();
    m_updated = {};
    m_usingCache = false;
    ++m_dataRevision;
    emit locationChanged();
    emit dataChanged();
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
    m_repository.fetch({m_latitude,m_longitude,m_model,m_requestedRun});
}

void WeatherModel::apply(const Meteo::Forecast &forecast)
{
    m_forecast = forecast;
    QHash<QString, double> scales;
    for (const auto &field : forecastFields()) scales.insert(field.key, displayScale(field.key, field.scale));
    m_series = buildChartSeries(forecast, scales);
    ++m_dataRevision;
    emit dataChanged();
}

bool WeatherModel::loadCache()
{
    const auto snapshot=m_repository.cached({m_latitude,m_longitude,m_model,m_requestedRun});
    if (!snapshot) return false;
    m_updated=snapshot->fetchedAt;
    m_usingCache=snapshot->usingCache;
    apply(snapshot->forecast);
    return true;
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
    const auto value = Meteo::forecastValueAt(*m_forecast, key, nearestTimestamp(summaryTimestamp()));
    return value ? QLocale().toString(*value * scale, 'f', 0) + suffix : QStringLiteral("—");
}

QString WeatherModel::temperatureText() const { return valueText("airtmp_point", QStringLiteral("°C")); }
QString WeatherModel::windText() const { return valueText("wind10_sd_true_prev_point", ' ' + m_windUnit, displayScale("wind10_sd_true_prev_point", 1)); }
QString WeatherModel::pressureText() const { return valueText("slpres_point", QStringLiteral(" hPa"), 0.01); }

QString WeatherModel::summaryTime() const
{
    if (!summaryTimestamp())
        return Meteo::i18n("No forecast for the current hour");
    const auto timestamp = nearestTimestamp(summaryTimestamp());
    return Meteo::i18n("Forecast for ") + formatTime(timestamp, QStringLiteral("ddd HH:mm"));
}

QString WeatherModel::weatherIcon() const
{
    if (!summaryTimestamp())
        return QStringLiteral("weather-none-available");
    const auto rain = Meteo::forecastValueAt(*m_forecast, "pcpttl_aver", nearestTimestamp(summaryTimestamp()));
    if (rain && *rain > 0.1)
        return QStringLiteral("weather-showers");
    const auto clouds = Meteo::forecastValueAt(*m_forecast, "cldtot_aver", nearestTimestamp(summaryTimestamp()));
    if (!clouds)
        return QStringLiteral("weather-none-available");
    if (*clouds > 0.8)
        return QStringLiteral("weather-overcast");
    if (*clouds > 0.25)
        return QStringLiteral("weather-clouds");
    return Meteo::nightAt(summaryTimestamp(), m_latitude, m_longitude)
        ? QStringLiteral("weather-clear-night") : QStringLiteral("weather-clear");
}

void WeatherModel::setRequestedRun(double value)
{
    if (!std::isfinite(value) || value<0 || value>253402300799.0) return;
    const auto run = qint64(value);
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
    else { ++m_dataRevision; emit dataChanged(); }
    emit stateChanged();
}

void WeatherModel::setUtc(bool value)
{
    if (m_utc == value) return;
    m_utc = value;
    ++m_dataRevision;
    emit dataChanged();
    emit stateChanged();
    emit runsChanged();
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
    timestamp = std::clamp(timestamp, firstTimestamp(), lastTimestamp());
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
        const auto sample = Meteo::sampleForecast(*m_forecast, field.key, time);
        const auto value = sample ? sample->value : std::nullopt;
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
        const bool instant = sample && sample->timing == Meteo::QuantityTiming::Instant;
        const auto source = sample ? QVariant(sample->sourceTimestamp) : QVariant();
        const auto from = sample && !instant ? QVariant(sample->intervalFrom) : QVariant();
        const auto to = sample && !instant ? QVariant(sample->intervalTo) : QVariant();
        const auto sourceText = !sample ? QString() : instant
            ? Meteo::i18n("Sample: %1").arg(formatTime(sample->sourceTimestamp, QStringLiteral("d MMM HH:mm")))
            : Meteo::i18n("Interval: %1–%2").arg(formatTime(sample->intervalFrom, QStringLiteral("d MMM HH:mm")),
                                               formatTime(sample->intervalTo, QStringLiteral("d MMM HH:mm")));
        details.append(QVariantMap{{"key", field.key}, {"label", field.label}, {"value", text}, {"group", field.group},
            {"number", value ? QVariant(*value * scale) : QVariant()}, {"unit", unit}, {"color", field.color},
            {"darkColor", forecastDarkColor(field.color, field.key)}, {"sourceTimestamp", source},
            {"intervalFrom", from}, {"intervalTo", to}, {"sampleText", sourceText},
            {"sampling", Meteo::timingName(Meteo::quantityTiming(field.key))}});
    }
    return details;
}

void WeatherModel::loadRuns()
{
    m_repository.loadRuns(m_model);
}

QVariantList WeatherModel::availableRuns() const
{
    QVariantList result;
    for (const auto timestamp : m_runs)
        result.append(QVariantMap{{"timestamp", timestamp}, {"label", timestamp
            ? formatTime(timestamp, QStringLiteral("ddd d MMM · HH:mm t")) : Meteo::i18n("Latest forecast")}});
    return result;
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
            const auto sample = Meteo::sampleForecast(*m_forecast, field.key, time);
            const auto value = sample ? sample->value : std::nullopt;
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
    for (const auto &span : Meteo::calendarSpans(qint64(from), qint64(to), zone))
        result.append(QVariantMap{{"start", span.start}, {"end", span.end}});
    return result;
}

QVariantList WeatherModel::nightIntervals() const
{
    QVariantList result;
    if (!m_forecast) return result;
    for (const auto &span : Meteo::nightsBetween(m_forecast->firstTimestamp(), m_forecast->lastTimestamp(), m_latitude, m_longitude))
        result.append(QVariantMap{{"start", span.start}, {"end", span.end}});
    return result;
}

QVariantMap WeatherModel::solarTimes() const
{
    if (!m_forecast) return {};
    const auto zone = m_utc ? QTimeZone::utc() : QTimeZone::systemTimeZone();
    const auto timestamp = summaryTimestamp();
    const auto day = QDateTime::fromSecsSinceEpoch(timestamp ? timestamp : m_forecast->firstTimestamp(), zone).date();
    const auto events = Meteo::solarEvents(day, zone, m_latitude, m_longitude);
    const auto sunrise = events.sunrise.value_or(0), sunset = events.sunset.value_or(0);
    return {{"date", day.toString(Qt::ISODate)}, {"dateLabel", QLocale().toString(day, QStringLiteral("d MMM"))},
        {"today", day == QDateTime::currentDateTimeUtc().toTimeZone(zone).date()},
        {"sunrise", sunrise}, {"sunset", sunset},
        {"sunriseText", sunrise ? formatTime(sunrise, "HH:mm") : QStringLiteral("—")},
        {"sunsetText", sunset ? formatTime(sunset, "HH:mm") : QStringLiteral("—")},
        {"status", !sunrise && !sunset ? (events.polarNight ? Meteo::i18n("Polar night") : Meteo::i18n("Polar day")) : QString()}};
}

QString WeatherModel::sunTimes() const
{
    const auto solar = solarTimes();
    if (solar.isEmpty()) return {};
    if (!solar["status"].toString().isEmpty()) return solar["status"].toString();
    return Meteo::i18n("%1 · sunrise ≈ %2 · sunset ≈ %3")
        .arg(solar["dateLabel"].toString(), solar["sunriseText"].toString(), solar["sunsetText"].toString());
}
