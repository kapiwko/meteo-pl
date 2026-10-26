#pragma once
#include "ForecastFields.hpp"
#include "core/Forecast.hpp"
#include <QTimeZone>
#include <QVariantMap>

// One description of a native sample for readings and both hourly views.
QVariantMap formatForecastSample(const Meteo::Forecast &forecast,
                                 const ForecastField &field, qint64 timestamp,
                                 double scale, const QString &windUnit,
                                 const QTimeZone &zone);
