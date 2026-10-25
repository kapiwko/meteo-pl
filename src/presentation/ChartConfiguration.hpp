#pragma once
#include <QVariantList>
#include <QVariantMap>

QVariantList chartSeriesCatalog();
QVariantMap defaultSeriesStyle(const QString &key);
QVariantMap normalizeSeriesStyle(const QVariantMap &value);
QVariantList defaultChartLayout(const QString &profile);
QVariantList normalizeChartLayout(const QVariantList &saved);
QStringList chartFieldKeys(const QVariantList &series);
QString seriesScale(const QString &key);
