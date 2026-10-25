#include "ForecastSampling.hpp"
#include <cmath>
#include <limits>

namespace Meteo {
QuantityTiming quantityTiming(const QString &key)
{
    if (key == "pcpttl_type_max") return QuantityTiming::Category;
    if (key == "pcpttl_aver") return QuantityTiming::IntervalTotal;
    if (key.endsWith("_min") || key.endsWith("_max")) return QuantityTiming::IntervalExtremum;
    if (key.endsWith("_aver")) return QuantityTiming::IntervalMean;
    return QuantityTiming::Instant;
}
QString timingName(QuantityTiming timing)
{
    switch (timing) {
    case QuantityTiming::Instant: return QStringLiteral("instant");
    case QuantityTiming::IntervalMean: return QStringLiteral("intervalMean");
    case QuantityTiming::IntervalTotal: return QStringLiteral("intervalTotal");
    case QuantityTiming::IntervalExtremum: return QStringLiteral("intervalExtremum");
    case QuantityTiming::Category: return QStringLiteral("category");
    }
    return {};
}
std::optional<ForecastSample> sampleSeries(const Series &series, qint64 timestamp, QuantityTiming timing)
{
    if (series.interval <= 0 || series.values.isEmpty()) return std::nullopt;
    const long double offset = static_cast<long double>(timestamp) - series.firstTimestamp;
    if (offset < 0) return std::nullopt;
    const long double length = static_cast<long double>(series.interval) * series.values.size();
    const bool instant = timing == QuantityTiming::Instant;
    if (instant ? offset > length-series.interval : offset >= length) return std::nullopt;
    const auto index = qsizetype(std::floor(offset/series.interval + (instant ? 0.5L : 0.0L)));
    const long double from = static_cast<long double>(series.firstTimestamp) + index*static_cast<long double>(series.interval);
    const long double to = from + (instant ? 0 : series.interval);
    if (from < std::numeric_limits<qint64>::min() || to > std::numeric_limits<qint64>::max()) return std::nullopt;
    return ForecastSample{series.values.at(index), qint64(from), qint64(from), qint64(to), timing};
}
std::optional<ForecastSample> sampleForecast(const Forecast &forecast, const QString &key, qint64 timestamp)
{
    const auto it = forecast.series.constFind(key);
    return it == forecast.series.cend() ? std::nullopt : sampleSeries(*it, timestamp, quantityTiming(key));
}
std::optional<double> forecastValueAt(const Forecast &forecast, const QString &key, qint64 timestamp)
{
    const auto sample = sampleForecast(forecast, key, timestamp);
    return sample ? sample->value : std::nullopt;
}
}
