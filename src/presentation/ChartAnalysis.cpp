#include "ChartAnalysis.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include "core/Localization.hpp"
#include <QLocale>
#include <QSet>

namespace {
QVariantMap toVariant(const ChartMath::AxisSpec &axis)
{
    QVariantList ticks;
    for (double value : axis.ticks) ticks.append(value);
    return {{"low", axis.low}, {"high", axis.high}, {"step", axis.step}, {"ticks", ticks}};
}
ChartMath::AxisSpec endpoints(double low, double high)
{
    return ChartMath::validRange(low, high) ? ChartMath::AxisSpec{low, high, high-low, {low, high}} : ChartMath::AxisSpec{};
}
}

bool ChartMath::validRange(double low, double high)
{
    return std::isfinite(low) && std::isfinite(high) && high > low && std::isfinite(high-low);
}

ChartMath::AxisSpec ChartMath::axis(double low, double high, double step)
{
    if (!validRange(low, high) || !std::isfinite(step) || step <= 0) return endpoints(low, high);
    constexpr int maximumTicks = 64;
    const long double span = static_cast<long double>(high)-low;
    if (span/step > maximumTicks-1 || low+step <= low) return endpoints(low, high);
    // Index multiplication avoids accumulated error; wider intermediates avoid
    // overflow when computing the first multiple. Never rely on float progress.
    const long double tolerance = span * 1e-9L;
    const long double first = std::ceil((static_cast<long double>(low)-tolerance)/step);
    if (!std::isfinite(first)) return endpoints(low, high);
    AxisSpec result{low, high, step, {}};
    for (int index = 0; index < maximumTicks; ++index) {
        const long double candidate = (first+index)*step;
        if (!std::isfinite(candidate) || candidate > static_cast<long double>(high)+tolerance) break;
        const double value = std::abs(candidate) <= tolerance && low <= 0 && high >= 0 ? 0.0 : static_cast<double>(candidate);
        if (!std::isfinite(value) || (!result.ticks.isEmpty() && value <= result.ticks.last())) break;
        if (candidate >= static_cast<long double>(low)-tolerance) result.ticks.append(value);
    }
    return result.ticks.isEmpty() ? endpoints(low, high) : result;
}

ChartMath::AxisSpec ChartMath::niceAxis(double low, double high)
{
    if (!std::isfinite(low) || !std::isfinite(high)) return {};
    if (!(high > low)) high = low + std::max(1.0, std::abs(low)*1e-6);
    if (!validRange(low, high)) return {};
    const long double raw = (static_cast<long double>(high)-low)/4;
    const long double magnitude = std::pow(10.0L, std::floor(std::log10(raw)));
    const auto normalized = raw/magnitude;
    const double step = static_cast<double>((normalized <= 1 ? 1 : normalized <= 2 ? 2 : normalized <= 5 ? 5 : 10)*magnitude);
    return axis(low, high, step);
}

QVariantMap ChartAnalysis::axis(double low, double high, double step) const { return toVariant(ChartMath::axis(low, high, step)); }
QVariantMap ChartAnalysis::niceAxis(double low, double high) const { return toVariant(ChartMath::niceAxis(low, high)); }
QVariantMap ChartAnalysis::precipitationAxis(double maximum) const
{
    const double high = maximum <= 5 ? 5 : maximum <= 10 ? 10 : maximum <= 20 ? 20 : 30;
    return axis(0, high, high/5);
}
QVariantMap ChartAnalysis::windAxis(double maximum, double scale) const
{
    double high = 50;
    for (double limit : {5,10,15,20,25,30,40,50}) if (maximum <= limit) { high = limit; break; }
    return axis(0, high*scale, (high <= 10 ? high/5 : high <= 30 ? 5 : 10)*scale);
}
QVariantMap ChartAnalysis::humidityAxis(double low, double high) const
{
    for (double step : {2,4,5,10,15,20,25}) {
        double start = std::floor(low/step)*step, end = start+5*step;
        if (high <= 100 && end > 100) { start = 100-5*step; end = 100; }
        if (start <= low && end >= high) return axis(start, end, step);
    }
    return axis(0, 100, 20);
}

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
