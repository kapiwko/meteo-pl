#include "ChartDataBuilder.hpp"
#include "core/ForecastSampling.hpp"

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
