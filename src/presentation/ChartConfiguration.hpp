#pragma once
#include <QVariantList>
#include <QVariantMap>

QVariantList chartSeriesCatalog();
QStringList seriesRenderers(const QString &key);
QStringList seriesFills(const QString &key);
QVariantMap defaultSeriesStyle(const QString &key);
QVariantMap normalizeSeriesStyle(const QVariantMap &value);
QVariantList defaultChartCatalog();
QVariantMap defaultChartAppearance();
QVariantMap defaultChartPrototype();
QVariantList builtinChartProfiles();
QVariantList defaultChartLayout(const QString &profile);
QVariantList normalizeChartLayout(const QVariantList &saved);
QVariantList removeUnusedChartTemplates(const QVariantList &saved);
QVariantList integrateWindDirection(const QVariantList &saved);
QStringList chartFieldKeys(const QVariantList &series);
QString seriesScale(const QString &key);
