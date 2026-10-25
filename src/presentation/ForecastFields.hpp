#pragma once
#include <QString>
#include <QVector>
#include <QStringList>

struct ForecastField {
    QString key, label, unit, group, color, kind;
    double scale = 1;
    int decimals = 1;
};
const QVector<ForecastField> &forecastFields();
QString forecastDarkColor(const QString &lightColor);
struct ForecastChart { QString key, title; QStringList fields; };
const QVector<ForecastChart> &forecastCharts();
