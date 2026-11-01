#pragma once
#include "core/Forecast.hpp"
#include <QHash>
#include <QVariantMap>

// Native sample axes and cross-series associations are prepared together.
// Painters receive values to draw, without independently interpreting sampling.
QVariantMap buildChartSeries(const Meteo::Forecast &forecast, const QHash<QString, double> &scales);
QVariantList prepareChartFields(const QVariantList &fields);
QVariantList buildConfiguredCharts(const QVariantList &layout, const QVariantMap &series, const QString &windUnit);
