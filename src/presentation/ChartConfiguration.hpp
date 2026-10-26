#pragma once
#include "MeteoUiExport.hpp"
#include <QVariantList>
#include <QVariantMap>

METEO_UI_EXPORT QVariantList chartSeriesCatalog();
METEO_UI_EXPORT QStringList seriesRenderers(const QString &key);
METEO_UI_EXPORT QVariantMap defaultSeriesStyle(const QString &key);
METEO_UI_EXPORT QVariantMap normalizeSeriesStyle(const QVariantMap &value);
METEO_UI_EXPORT QVariantList defaultChartLayout(const QString &profile);
METEO_UI_EXPORT QVariantList normalizeChartLayout(const QVariantList &saved);
METEO_UI_EXPORT QVariantList integrateWindDirection(const QVariantList &saved);
METEO_UI_EXPORT QStringList chartFieldKeys(const QVariantList &series);
METEO_UI_EXPORT QString seriesScale(const QString &key);
