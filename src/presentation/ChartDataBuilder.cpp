#include "ChartDataBuilder.hpp"
#include "PreparedChart.hpp"
#include "core/ForecastSampling.hpp"
#include "ChartConfiguration.hpp"
#include "ForecastFields.hpp"
#include "core/Localization.hpp"
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

QVariantList buildConfiguredCharts(const QVariantList &layout, const QVariantMap &sourceSeries, const QString &windUnit)
{
    QVariantList result;
    const auto numeric = [](const QVariantMap &style) {
        return !QStringList{"hidden", "type", "direction", "storm", "flash"}.contains(style.value("render").toString());
    };
    const auto unitFor = [&windUnit](const QVariantMap &style) {
        return style.value("unit") == "m/s" ? windUnit : style.value("unit").toString();
    };
    for (const auto &entry : normalizeChartLayout(layout)) {
        auto chart = entry.toMap();
        if (!chart.value("enabled").toBool()) continue;
        const auto styles = chart.value("series").toList();
        QStringList units[2], allUnits;
        QString scales[2];
        // Reserve explicit assignments before resolving auto. They are expert
        // choices, including deliberately shared axes with different units.
        for (const auto &value : styles) {
            const auto style = value.toMap();
            if (!style.value("enabled").toBool() || !sourceSeries.contains(style.value("key").toString()) || !numeric(style)) continue;
            const auto axis = style.value("axis").toString(), unit = unitFor(style);
            if (axis == "auto") continue;
            auto &assigned = units[axis == "right" ? 1 : 0];
            if (!assigned.contains(unit)) assigned.append(unit);
        }
        if (units[0].isEmpty()) {
            for (const auto &value : styles) {
                const auto style = value.toMap();
                if (style.value("enabled").toBool() && style.value("axis") != "right" && numeric(style)
                    && sourceSeries.contains(style.value("key").toString())) {
                    units[0].append(unitFor(style)); break;
                }
            }
        }
        QVariantList fields;
        bool conflict = false;
        for (const auto &value : styles) {
            auto style = value.toMap();
            const auto key = style.value("key").toString(), unit = unitFor(style);
            if (!style.value("enabled").toBool() || !sourceSeries.contains(key)) continue;
            auto axis = style.value("axis").toString();
            const bool quantity = numeric(style);
            if (quantity && !allUnits.contains(unit)) allUnits.append(unit);
            if (axis == "auto") {
                if (!quantity || units[0].contains(unit)) axis = "left";
                else if (units[1].contains(unit)) axis = "right";
                else if (units[0].isEmpty()) axis = "left";
                else if (units[1].isEmpty()) axis = "right";
                else { conflict = true; axis = "unassigned"; }
            }
            style["axis"] = axis; style["unit"] = unit; style["kind"] = style.value("render");
            style["points"] = sourceSeries.value(key);
            if (style.value("render") == "range" || style.value("render") == "rangeStems") {
                const auto lower = sourceSeries.value(style.value("lowerKey").toString()).toList();
                if (lower.isEmpty()) style["kind"] = "line";
                else style["lowerPoints"] = lower;
            }
            fields.append(style);
            if (!quantity || axis == "unassigned") continue;
            const int side = axis == "right" ? 1 : 0;
            if (!units[side].contains(unit)) units[side].append(unit);
            if (scales[side].isEmpty()) scales[side] = seriesScale(key);
        }
        chart["fields"] = fields;
        for (int side = 0; side < 2; ++side) {
            const auto prefix = side ? QStringLiteral("right") : QStringLiteral("left");
            if (chart.value(prefix + "Scale") == "auto")
                chart[prefix + "Scale"] = units[side].size() > 1 ? QStringLiteral("linear")
                    : scales[side].isEmpty() ? (side ? QStringLiteral("linked") : QStringLiteral("linear")) : scales[side];
            chart[prefix + "Unit"] = units[side].join(QStringLiteral(" / "));
        }
        allUnits.sort();
        chart["axisConflict"] = conflict ? Meteo::i18n("Automatic axes support two units. Assign axes explicitly or split this chart (%1).")
            .arg(allUnits.join(QStringLiteral(", "))) : QString();
        if (!fields.isEmpty()) {
            const auto prepared=describeChart(fields,chart,windUnit,chart.value("leftScale").toString());
            for (auto it=prepared.cbegin();it!=prepared.cend();++it) chart[it.key()]=it.value();
            result.append(chart);
        }
    }
    return result;
}
