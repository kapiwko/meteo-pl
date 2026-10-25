#pragma once
#include "MeteoUiExport.hpp"
#include <QObject>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqmlregistration.h>

namespace ChartMath {
struct AxisSpec {
    double low = 0, high = 1, step = 0.25;
    QVector<double> ticks{0, 0.25, 0.5, 0.75, 1};
};
METEO_UI_EXPORT bool validRange(double low, double high);
METEO_UI_EXPORT AxisSpec axis(double low, double high, double step);
METEO_UI_EXPORT AxisSpec niceAxis(double low, double high);
}

class METEO_UI_EXPORT ChartAnalysis : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    explicit ChartAnalysis(QObject *parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE QVariantMap axis(double low, double high, double step) const;
    Q_INVOKABLE QVariantMap niceAxis(double low, double high) const;
    Q_INVOKABLE QVariantMap precipitationAxis(double maximum) const;
    Q_INVOKABLE QVariantMap windAxis(double maximum, double scale) const;
    Q_INVOKABLE QVariantMap humidityAxis(double low, double high) const;
};
