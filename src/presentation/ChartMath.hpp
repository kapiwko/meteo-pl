#pragma once
#include "MeteoUiExport.hpp"
#include <QVariantMap>
#include <QVector>

namespace ChartMath {
struct AxisSpec {
    double low = 0, high = 1, step = 0.25;
    QVector<double> ticks{0, 0.25, 0.5, 0.75, 1};
};
METEO_UI_EXPORT QVariantMap toVariant(const AxisSpec &axis);
METEO_UI_EXPORT AxisSpec precipitationAxis(double maximum);
METEO_UI_EXPORT AxisSpec windAxis(double maximum,double scale);
METEO_UI_EXPORT AxisSpec humidityAxis(double low,double high);
METEO_UI_EXPORT bool validRange(double low, double high);
METEO_UI_EXPORT AxisSpec axis(double low, double high, double step);
METEO_UI_EXPORT AxisSpec niceAxis(double low, double high);
}
