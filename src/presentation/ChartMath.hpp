#pragma once
#include <QVariantMap>
#include <QVector>

namespace ChartMath {
struct AxisSpec {
    double low = 0, high = 1, step = 0.25;
    QVector<double> ticks{0, 0.25, 0.5, 0.75, 1};
};
QVariantMap toVariant(const AxisSpec &axis);
AxisSpec precipitationAxis(double maximum);
AxisSpec windAxis(double maximum,double scale);
AxisSpec humidityAxis(double low,double high);
bool validRange(double low, double high);
AxisSpec axis(double low, double high, double step);
AxisSpec niceAxis(double low, double high);
}
