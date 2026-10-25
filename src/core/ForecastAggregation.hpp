#pragma once
#include "Forecast.hpp"
#include <QTimeZone>

namespace Meteo {
struct DailyForecast {
    QDate date;
    std::optional<double> minimum, maximum, rain, gust, cloudCover;
    qint64 rainCoverageSeconds = 0;
    bool rainComplete = false;
    bool rainEstimatedAtBoundary = false;
    bool storm = false, snow = false, mixed = false;
};
// Totals use each source interval once. At a calendar boundary, distribute an
// interval total by elapsed seconds and flag that within-interval estimate.
QVector<DailyForecast> aggregateDays(const Forecast &forecast, const QTimeZone &zone);
}
