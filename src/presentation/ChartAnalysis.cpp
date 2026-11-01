#include "ChartAnalysis.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include "core/Localization.hpp"
#include "PreparedChart.hpp"
#include <QLocale>
#include <QSet>

QVariantMap ChartAnalysis::axis(double low, double high, double step) const { return ChartMath::toVariant(ChartMath::axis(low, high, step)); }
QVariantMap ChartAnalysis::niceAxis(double low, double high) const { return ChartMath::toVariant(ChartMath::niceAxis(low, high)); }
QVariantMap ChartAnalysis::precipitationAxis(double maximum) const { return ChartMath::toVariant(ChartMath::precipitationAxis(maximum)); }
QVariantMap ChartAnalysis::windAxis(double maximum,double scale) const { return ChartMath::toVariant(ChartMath::windAxis(maximum,scale)); }
QVariantMap ChartAnalysis::humidityAxis(double low,double high) const { return ChartMath::toVariant(ChartMath::humidityAxis(low,high)); }

QString ChartAnalysis::precipitationCaption(const QVariantList &fields) const
{
    QMap<QString, QSet<qint64>> periods;
    for (const auto &entry : fields) {
        const auto field = entry.toMap();
        const auto key = field.value("key").toString();
        if (key != "pcpttl_aver" && key != "pcpttl_max") continue;
        for (const auto &value : field.value("points").toList()) {
            const auto point = value.toMap();
            const auto duration = point.value("intervalFrom").isValid() && point.value("intervalTo").isValid()
                ? point.value("intervalTo").toLongLong() - point.value("intervalFrom").toLongLong()
                : point.value("interval").toLongLong();
            if (duration > 0) periods[key].insert(duration);
        }
    }
    if (periods.isEmpty()) return {};
    const auto hours = [](const QSet<qint64> &values) {
        auto ordered = values.values(); std::sort(ordered.begin(), ordered.end());
        QStringList text;
        for (const auto seconds : ordered) text.append(QLocale().toString(seconds / 3600.0, 'g', 8));
        return text.join(QStringLiteral(" / "));
    };
    if (periods.size() == 1 || periods.first() == periods.last())
        return Meteo::i18n("Precipitation totals over %1 h.").arg(hours(periods.first()));
    return Meteo::i18n("Mean: total over %1 h. Maximum: total over %2 h.")
        .arg(hours(periods.value("pcpttl_aver")), hours(periods.value("pcpttl_max")));
}

QVariantList ChartAnalysis::prepareFields(const QVariantList &fields) const { return prepareChartFields(fields); }

QVariantMap ChartAnalysis::describe(const QVariantList &fields,const QVariantMap &configuration,const QString &windUnit,const QString &fallback) const
{ return describeChart(fields,configuration,windUnit,fallback); }
QVariantMap ChartAnalysis::probeSamples(const QVariantList &samples,const QVariantList &fields) const
{ return prepareProbeSamples(samples,fields); }
