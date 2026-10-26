#pragma once
#include "MeteoUiExport.hpp"
#include "core/Forecast.hpp"
#include <QHash>
#include <QVariantMap>

// Native sample axes and cross-series associations are prepared together.
// Painters receive values to draw, without independently interpreting sampling.
METEO_UI_EXPORT QVariantMap buildChartSeries(const Meteo::Forecast &forecast, const QHash<QString, double> &scales);
METEO_UI_EXPORT QVariantList prepareChartFields(const QVariantList &fields);
