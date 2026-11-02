#include "PreparedChart.hpp"
#include "PreparedLegend.hpp"
#include "ChartConfiguration.hpp"
#include "ChartMath.hpp"
#include "core/Localization.hpp"
#include <QLocale>
#include <cmath>
#include <limits>
#include <algorithm>

SeriesCapabilities seriesCapabilities(const QString &key)
{ return {seriesScale(key),seriesRenderers(key),seriesFills(key),Meteo::quantityTiming(key)}; }
namespace {
bool number(const QVariant &value) { return value.isValid() && !value.isNull() && std::isfinite(value.toDouble()); }
bool rightField(const QVariantMap &field) { return field.value("axis")=="right"; }
struct PreparedAxis {
    ChartMath::AxisSpec range;
    QString kind,unit;
    QVector<double> levels;
    double labelFactor=1;
    bool linked=false;
    QVariantMap variant() const {
        QVariantList ticks,labels,transform;
        for (double value : levels) transform.append(value);
        for (double value : levels.isEmpty()?range.ticks:levels) {
            ticks.append(value);
            value*=labelFactor;
            QString text;
            if (kind=="fraction") text=QString::number(value/100,'f',2);
            else if (kind=="clouds") text=QString::number(value*8/100,'f',0);
            else if (kind=="cloudHeights") text=QString::number(value,'f',1);
            else text=QLocale().toString(value,'f',std::abs(value-std::round(value))<0.0001?0:1);
            labels.append(text);
        }
        return {{"range",ChartMath::toVariant(range)},{"kind",kind},{"unit",unit},{"levels",transform},
                {"ticks",ticks},{"labels",labels},{"labelFactor",labelFactor},{"linked",linked}};
    }
};
ChartMath::AxisSpec configured(ChartMath::AxisSpec axis,const QVariantMap &configuration,const QString &prefix)
{
    const auto minimum=configuration.value(prefix+"Min"),maximum=configuration.value(prefix+"Max");
    const auto low=number(minimum)?minimum.toDouble():axis.low,high=number(maximum)?maximum.toDouble():axis.high;
    return (number(minimum)||number(maximum)) && ChartMath::validRange(low,high) ? ChartMath::niceAxis(low,high) : axis;
}
PreparedAxis axisFor(const QVariantList &fields,const QVariantMap &configuration,bool right,const QString &kind,double windScale)
{
    PreparedAxis result; result.kind=kind;
    const QString prefix=right?"right":"left";
    double low=std::numeric_limits<double>::infinity(),high=-low,speeds=0,gusts=0; bool hasSpeed=false;
    for (const auto &value : fields) {
        const auto field=value.toMap();
        if (QStringList{"hidden","direction","type","storm","flash"}.contains(field.value("kind").toString())) continue;
        if (rightField(field)!=right) continue;
        if (result.unit.isEmpty()) result.unit=field.value("unit").toString();
        auto points=field.value("points").toList(); points.append(field.value("lowerPoints").toList());
        for (const auto &entry : points) {
            const auto point=entry.toMap(); const auto value=point.value("value"); if (!number(value)) continue;
            low=std::min(low,value.toDouble()); high=std::max(high,value.toDouble());
            if (field.value("key")=="wind10_sd_true_prev_point") { hasSpeed=true; speeds=std::max(speeds,value.toDouble()); }
            if (field.value("key")=="wind_gust_max") gusts=std::max(gusts,value.toDouble());
        }
    }
    if (!std::isfinite(low)) { low=0; high=1; }
    if (kind=="direction") result.range=ChartMath::axis(0,360,90);
    else if (kind=="probability") result.range=ChartMath::axis(0,100,20);
    else if (kind=="cloudHeights") { result.range=ChartMath::axis(0,15,1); result.levels={0,0.5,2,7,15}; }
    else if (kind=="visibility") { result.range=ChartMath::axis(0,100,25); result.levels={0,1,5,20,100}; }
    else if (kind=="fraction" || kind=="clouds") result.range=ChartMath::axis(0,100,25);
    else if (kind=="humidity") result.range=ChartMath::humidityAxis(low,high);
    else if (kind=="precipitation") result.range=ChartMath::precipitationAxis(high);
    else if (kind=="wind") result.range=ChartMath::windAxis((hasSpeed?std::min(std::max(speeds,gusts),speeds*4):gusts)/windScale,windScale);
    else if (kind=="temperature") result.range=ChartMath::niceAxis(low-1,high+1);
    else if (kind=="pressure") result.range=ChartMath::niceAxis(std::round(low-std::max(0.0,15-(high-low))-2),std::round(high+2));
    else result.range=ChartMath::niceAxis(std::min(0.0,low),std::max(1.0,high));
    result.range=configured(result.range,configuration,prefix);
    if (number(configuration.value(prefix+"Min")) || number(configuration.value(prefix+"Max"))) result.levels.clear();
    if (configuration.contains(prefix+"Unit")) result.unit=configuration.value(prefix+"Unit").toString();
    if (kind=="clouds") result.unit=Meteo::i18n("octas");
    if (kind=="fraction") result.unit=Meteo::i18n("fraction");
    return result;
}
}
QVariantMap describeChart(const QVariantList &input,const QVariantMap &configuration,const QString &windUnit,const QString &fallbackScale)
{
    const auto fields=prepareChartFields(input);
    const auto scale=configuration.value("leftScale",fallbackScale=="ground"?"temperature":fallbackScale).toString();
    const auto rightScale=configuration.value("rightScale",scale=="clouds"?"fraction":scale=="cloudHeights"?"visibility":scale=="precipitation"?"humidity":"linked").toString();
    const double windScale=windUnit=="km/h"?3.6:windUnit=="kt"?1.943844492:1;
    auto left=axisFor(fields,configuration,false,scale,windScale),right=axisFor(fields,configuration,true,rightScale,windScale);
    if (rightScale=="linked") {
        right=left; right.range=configured(left.range,configuration,"right");
        // Manual limits detach the range while preserving alternate-unit labels.
        // Every consumer uses this axis's range, ticks and unit conversion.
        right.linked=!number(configuration.value("rightMin")) && !number(configuration.value("rightMax"));
        if (scale=="pressure") { right.labelFactor=0.75; right.unit="mm Hg"; }
        else if (scale=="wind") { right.labelFactor=windUnit=="m/s"?3.6:windUnit=="km/h"?1/3.6:1/1.943844492; right.unit=windUnit=="m/s"?"km/h":"m/s"; }
    }
    bool hasRight=QStringList{"temperature","pressure","wind","probability","clouds"}.contains(scale),directionOnly=!fields.isEmpty();
    for (const auto &value : fields) {
        const auto field=value.toMap(); const auto kind=field.value("kind").toString();
        hasRight |= rightField(field); directionOnly &= kind=="direction" || kind=="hidden";
    }
    auto result=prepareChartLegend(fields,left.range,right.range,windScale);
    result.insert("fields",fields); result.insert("leftAxis",left.variant()); result.insert("rightAxis",right.variant());
    result.insert("hasRightAxis",hasRight); result.insert("directionOnly",directionOnly);
    result.insert("prepared",true); result.insert("windScale",windScale);
    return result;
}
QVariantMap prepareProbeSamples(const QVariantList &samples,const QVariantList &fields)
{
    QVariantMap result;
    for (const auto &value : samples) {
        auto sample=value.toMap(); const auto key=sample.value("key").toString();
        for (const auto &entry : fields) {
            const auto field=entry.toMap(); if (field.value("key")!=key) continue;
            const bool center=field.value("renderTimePlacement")=="center" && number(sample.value("intervalFrom")) && number(sample.value("intervalTo"));
            sample["renderTimestamp"]=center?(sample["intervalFrom"].toDouble()+sample["intervalTo"].toDouble())/2:sample["sourceTimestamp"].toDouble();
            result[key]=sample; break;
        }
    }
    return result;
}
