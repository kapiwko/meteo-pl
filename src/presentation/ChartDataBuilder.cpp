#include "ChartDataBuilder.hpp"
#include "core/ForecastSampling.hpp"
#include <cmath>
#include <QSet>
#include <algorithm>

QVariantMap buildChartSeries(const Meteo::Forecast &forecast, const QHash<QString, double> &scales)
{
    QVariantMap result;
    const auto numberAt = [&](const QString &key, qint64 timestamp) -> QVariant {
        const auto sample = Meteo::sampleForecast(forecast, key, timestamp);
        return sample && sample->value ? QVariant(*sample->value) : QVariant();
    };
    for (auto it = forecast.series.cbegin(); it != forecast.series.cend(); ++it) {
        const auto &series = it.value();
        QVariantList points;
        for (qsizetype i = 0; i < series.values.size(); ++i) {
            const auto time = series.firstTimestamp + qint64(i) * series.interval;
            const auto value = series.values[i];
            QVariantMap point{{"time", time}, {"value", value ? QVariant(*value * scales.value(it.key(), 1)) : QVariant()},
                              {"interval", series.interval}};
            const auto timing = Meteo::quantityTiming(it.key());
            const auto sample = Meteo::sampleSeries(series, time, timing);
            point["sourceTimestamp"] = time;
            point["sampling"] = Meteo::timingName(timing);
            point["intervalFrom"] = sample && timing != Meteo::QuantityTiming::Instant ? QVariant(sample->intervalFrom) : QVariant();
            point["intervalTo"] = sample && timing != Meteo::QuantityTiming::Instant ? QVariant(sample->intervalTo) : QVariant();
            if (it.key() == "pcpttl_aver" || it.key() == "pcpttl_max")
                point["precipitationType"] = numberAt("pcpttl_type_max", time);
            if (it.key() == "storm_max" || it.key() == "flash_max") {
                point["storm"] = numberAt("storm_max", time);
                point["flash"] = numberAt("flash_max", time);
            }
            points.append(point);
        }
        result.insert(it.key(), points);
    }
    return result;
}

namespace {
QVariantList chartEvents(const QVariantList &fields)
{
    struct Source { qint64 from, to; QVariant value; bool flash; };
    QList<Source> sources;
    QSet<qint64> boundaries;
    const auto positive = [](const QVariant &value) {
        return value.isValid() && !value.isNull() && std::isfinite(value.toDouble()) && value.toDouble() > 0;
    };
    for (const auto &entry : fields) {
        const auto field = entry.toMap();
        const auto kind = field.value("kind", field.value("render")).toString();
        if (kind != "storm" && kind != "flash") continue;
        for (const auto &value : field.value("points").toList()) {
            const auto point = value.toMap();
            const auto from = point.value("intervalFrom").isValid() && !point.value("intervalFrom").isNull()
                ? point.value("intervalFrom").toLongLong() : point.value("time").toLongLong();
            const auto to = point.value("intervalTo").isValid() && !point.value("intervalTo").isNull()
                ? point.value("intervalTo").toLongLong() : from + point.value("interval").toLongLong();
            if (to <= from) continue;
            sources.append({from, to, point.value("value"), kind == "flash"});
            if (positive(point.value("value"))) { boundaries.insert(from); boundaries.insert(to); }
        }
    }
    auto times = boundaries.values(); std::sort(times.begin(), times.end());
    QVariantList events;
    QVariant previousStormSource, previousFlashSource;
    for (qsizetype i = 1; i < times.size(); ++i) {
        const auto from = times[i-1], to = times[i];
        QVariant storm, flash, stormSource, flashSource;
        for (const auto &source : sources) {
            if (source.from > from || source.to < to) continue;
            auto &value = source.flash ? flash : storm;
            auto &origin = source.flash ? flashSource : stormSource;
            value = source.value;
            if (positive(value)) origin = source.from;
        }
        if (!positive(storm) && !positive(flash)) continue;
        if (!events.isEmpty()) {
            auto previous = events.last().toMap();
            if (previous.value("intervalTo").toLongLong() == from && previous.value("storm") == storm
                && previous.value("flash") == flash && previousStormSource == stormSource && previousFlashSource == flashSource) {
                previous["intervalTo"] = to;
                previous["interval"] = to - previous.value("intervalFrom").toLongLong();
                events.last() = previous;
                continue;
            }
        }
        events.append(QVariantMap{{"time", from}, {"intervalFrom", from}, {"intervalTo", to}, {"interval", to-from},
                                  {"storm", storm}, {"flash", flash}});
        previousStormSource = stormSource; previousFlashSource = flashSource;
    }
    return events;
}
}

QVariantList prepareChartFields(const QVariantList &fields)
{
    QVariantList result;
    const auto number = [](const QVariant &value) { return value.isValid() && !value.isNull() && std::isfinite(value.toDouble()); };
    for (const auto &entry : fields) {
        auto field = entry.toMap();
        const auto kind = field.value("kind", field.value("render")).toString();
        if ((kind == "range" || kind == "rangeStems") && !field.contains("rangeSegments")) {
            const auto lower = field.value("lowerPoints").toList();
            QMap<qint64, QVariant> upper;
            for (const auto &point : field.value("points").toList()) {
                const auto map = point.toMap(); upper.insert(map.value("time").toLongLong(), map.value("value"));
            }
            QVariantList segments, pairs;
            QVariantMap previous;
            bool connected = false;
            for (const auto &value : lower) {
                const auto point = value.toMap();
                const auto time = point.value("time").toLongLong();
                const auto high = upper.constFind(time);
                if (high == upper.cend() || !number(*high) || !number(point.value("value"))) { connected = false; continue; }
                const QVariantMap pair{{"time", time}, {"lower", point.value("value")}, {"upper", *high}};
                pairs.append(pair);
                if (connected && time > previous.value("time").toLongLong()) {
                    bool gap = false;
                    for (auto it = upper.upperBound(previous.value("time").toLongLong()); it != upper.end() && it.key() < time; ++it)
                        if (!number(it.value())) { gap = true; break; }
                    if (!gap) segments.append(QVariantMap{{"from", previous}, {"to", pair}});
                }
                previous = pair; connected = true;
            }
            field["rangeSegments"] = segments;
            field["rangePairs"] = pairs;
        }
        result.append(field);
    }
    const auto events = chartEvents(result);
    for (auto &entry : result) {
        auto field = entry.toMap();
        const auto kind = field.value("kind", field.value("render")).toString();
        if (kind == "storm" || kind == "flash") { field["events"] = events; entry = field; }
    }
    return result;
}
