#pragma once
#include "Forecast.hpp"

namespace Meteo {
// Sampling is a property of the quantity, independent of its chosen chart style.
enum class QuantityTiming { Instant, IntervalStatistic, IntervalTotal, IntervalExtremum, Category };
struct ForecastSample {
    std::optional<double> value;
    qint64 sourceTimestamp;
    qint64 intervalFrom, intervalTo;
    QuantityTiming timing;
};

[[nodiscard]] QuantityTiming quantityTiming(const QString &key);
[[nodiscard]] QString timingName(QuantityTiming timing);
// Instantaneous values use the nearest native sample (ties go forward), within
// the native time axis only. Interval quantities use [source, source + interval).
// Nulls remain null; neither policy fills gaps or extrapolates.
[[nodiscard]] std::optional<ForecastSample> sampleSeries(const Series &series, qint64 timestamp, QuantityTiming timing);
[[nodiscard]] std::optional<ForecastSample> sampleForecast(const Forecast &forecast, const QString &key, qint64 timestamp);
[[nodiscard]] std::optional<double> forecastValueAt(const Forecast &forecast, const QString &key, qint64 timestamp);
}
