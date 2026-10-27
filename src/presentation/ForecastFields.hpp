#pragma once
#include "MeteoUiExport.hpp"
#include <QString>
#include <QVector>
#include <QStringList>

struct ForecastField {
    QString key, label, unit, group, color, kind;
    double scale = 1;
    int decimals = 1;
    QString darkColor, axisScale;
    QStringList renderers;
};
METEO_UI_EXPORT const QVector<ForecastField> &forecastFields();
METEO_UI_EXPORT QString forecastDarkColor(const QString &lightColor, const QString &fieldKey = {});
struct ForecastChart { QString key, title; QStringList fields; };
METEO_UI_EXPORT const QVector<ForecastChart> &forecastCharts();
