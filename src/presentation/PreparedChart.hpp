#pragma once
#include "MeteoUiExport.hpp"
#include "core/ForecastSampling.hpp"
#include <QVariantMap>

// Domain roles, temporal meaning and drawing geometry are distinct contracts.
enum class SeriesGeometry { Line, Points, Area, Bars, Stems, Segments, Direction, Range, RangeStems, CloudArea, CloudPoint, Type, Storm, Flash, Hidden };
struct SeriesCapabilities {
    QString scale;
    QStringList renderers, fills;
    Meteo::QuantityTiming timing=Meteo::QuantityTiming::Instant;
};
METEO_UI_EXPORT SeriesCapabilities seriesCapabilities(const QString &key);
METEO_UI_EXPORT QVariantList prepareChartFields(const QVariantList &fields);
METEO_UI_EXPORT QVariantMap describeChart(const QVariantList &fields,const QVariantMap &configuration,const QString &windUnit,const QString &fallbackScale);
METEO_UI_EXPORT QVariantMap prepareProbeSamples(const QVariantList &samples,const QVariantList &fields);
