#pragma once
#include "ChartMath.hpp"
#include <QVariantMap>

// Legend meanings and color intervals are prepared alongside the chart axes.
// QML only lays out these entries and draws their samples.
QVariantMap prepareChartLegend(const QVariantList &fields, const ChartMath::AxisSpec &left,
                              const ChartMath::AxisSpec &right, double windScale);
