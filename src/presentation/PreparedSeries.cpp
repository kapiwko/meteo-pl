#include "PreparedChart.hpp"
#include "ChartConfiguration.hpp"
#include "ForecastFields.hpp"
#include <QSet>
#include <cmath>
#include <algorithm>

namespace {
SeriesGeometry geometryFor(const QString &name)
{
    static const QMap<QString,SeriesGeometry> kinds{{"line",SeriesGeometry::Line},{"points",SeriesGeometry::Points},
        {"area",SeriesGeometry::Area},{"bars",SeriesGeometry::Bars},{"stems",SeriesGeometry::Stems},
        {"gusts",SeriesGeometry::Segments},{"direction",SeriesGeometry::Direction},{"range",SeriesGeometry::Range},
        {"rangeStems",SeriesGeometry::RangeStems},{"cloudArea",SeriesGeometry::CloudArea},{"cloudPoint",SeriesGeometry::CloudPoint},
        {"type",SeriesGeometry::Type},{"storm",SeriesGeometry::Storm},{"flash",SeriesGeometry::Flash},{"hidden",SeriesGeometry::Hidden}};
    return kinds.value(name,SeriesGeometry::Line);
}
}
namespace {
QVariantList chartEvents(const QVariantList &fields)
{
    struct Source { qint64 from, to; QVariant value; bool flash; };
    QList<Source> sources;
    QSet<qint64> boundaries;
    const auto positive = [](const QVariant &value) {
        return value.isValid() && !value.isNull() && std::isfinite(value.toDouble()) && value.toDouble() > 0;
    };
    for (const auto &entry : fields) {
        const auto field = entry.toMap();
        const auto kind = field.value("kind", field.value("render")).toString();
        if (kind != "storm" && kind != "flash") continue;
        for (const auto &value : field.value("points").toList()) {
            const auto point = value.toMap();
            const auto from = point.value("intervalFrom").isValid() && !point.value("intervalFrom").isNull()
                ? point.value("intervalFrom").toLongLong() : point.value("time").toLongLong();
            const auto to = point.value("intervalTo").isValid() && !point.value("intervalTo").isNull()
                ? point.value("intervalTo").toLongLong() : from + point.value("interval").toLongLong();
            if (to <= from) continue;
            sources.append({from, to, point.value("value"), kind == "flash"});
            if (positive(point.value("value"))) { boundaries.insert(from); boundaries.insert(to); }
        }
    }
    auto times = boundaries.values(); std::sort(times.begin(), times.end());
    QVariantList events;
    QVariant previousStormSource, previousFlashSource;
    for (qsizetype i = 1; i < times.size(); ++i) {
        const auto from = times[i-1], to = times[i];
        QVariant storm, flash, stormSource, flashSource;
        for (const auto &source : sources) {
            if (source.from > from || source.to < to) continue;
            auto &value = source.flash ? flash : storm;
            auto &origin = source.flash ? flashSource : stormSource;
            value = source.value;
            if (positive(value)) origin = source.from;
        }
        if (!positive(storm) && !positive(flash)) continue;
        if (!events.isEmpty()) {
            auto previous = events.last().toMap();
            if (previous.value("intervalTo").toLongLong() == from && previous.value("storm") == storm
                && previous.value("flash") == flash && previousStormSource == stormSource && previousFlashSource == flashSource) {
                previous["intervalTo"] = to;
                previous["interval"] = to - previous.value("intervalFrom").toLongLong();
                events.last() = previous;
                continue;
            }
        }
        events.append(QVariantMap{{"time", from}, {"intervalFrom", from}, {"intervalTo", to}, {"interval", to-from},
                                  {"storm", storm}, {"flash", flash}});
        previousStormSource = stormSource; previousFlashSource = flashSource;
    }
    return events;
}
}

QVariantList prepareChartFields(const QVariantList &fields)
{
    if (std::all_of(fields.cbegin(),fields.cend(),[](const QVariant &entry){return entry.toMap().value("prepared").toBool();})) return fields;
    QVariantList result;
    const auto number = [](const QVariant &value) { return value.isValid() && !value.isNull() && std::isfinite(value.toDouble()); };
    for (const auto &entry : fields) {
        const auto supplied=entry.toMap();
        auto field=defaultSeriesStyle(supplied.value("key").toString());
        for (auto it=supplied.cbegin();it!=supplied.cend();++it) field[it.key()]=it.value();
        auto kind=supplied.value("kind",field.value("render")).toString();
        if (kind=="upper" || kind=="feelUpper") kind="range";
        if (kind=="groundHigh") kind="rangeStems";
        field["kind"]=kind; field["geometry"]=kind;
        if (!supplied.contains("axis")) field["axis"]=(kind=="right" || kind=="rightBars")?"right":"left";
        const auto key=field.value("key").toString();
        for (const auto &metadata : forecastFields()) if (metadata.key==key) field["quantityRole"]=metadata.kind;
        const auto geometry=geometryFor(kind);
        const auto capabilities=seriesCapabilities(key);
        const bool center=capabilities.timing==Meteo::QuantityTiming::IntervalTotal || geometry==SeriesGeometry::Segments;
        const auto timing=capabilities.timing;
        field["renderTimePlacement"] = center && timing!=Meteo::QuantityTiming::Instant ? "center" : "source";
        QVariantList points;
        bool connected=false;
        double previousValue=0;
        for (const auto &value : field.value("points").toList()) {
            auto point=value.toMap();
            const bool valid=number(point.value("value"));
            const double current=point.value("value").toDouble();
            point["connectPrevious"]=valid && connected
                && !(capabilities.scale=="direction" && std::abs(current-previousValue)>180);
            connected=valid;
            previousValue=current;
            const auto time=point.value("sourceTimestamp",point.value("time")).toDouble();
            const auto interval=point.value("interval").toDouble();
            const bool instant=timing==Meteo::QuantityTiming::Instant;
            const auto from=point.value("intervalFrom").isNull() || !point.value("intervalFrom").isValid()
                ? time-(instant && kind=="gusts"?interval/2:0) : point["intervalFrom"].toDouble();
            const auto to=point.value("intervalTo").isNull() || !point.value("intervalTo").isValid() ? from+interval : point["intervalTo"].toDouble();
            point["renderTimestamp"]=center && !instant ? (from+to)/2 : time;
            point["segmentFrom"]=from; point["segmentTo"]=to;
            points.append(point);
        }
        field["points"]=points;
        field["toggleKeys"]=QStringList{key};
        if ((kind=="range" || kind=="rangeStems") && !field.value("lowerKey").toString().isEmpty()) field["toggleKeys"]=QStringList{key,field["lowerKey"].toString()};
        if (kind=="storm" || kind=="flash") field["toggleKeys"]=QStringList{"storm_max","flash_max"};
        field["prepared"]=true;
        if ((kind == "range" || kind == "rangeStems") && !field.contains("rangeSegments")) {
            const auto lower = field.value("lowerPoints").toList();
            QMap<qint64, QVariant> upper;
            for (const auto &point : field.value("points").toList()) {
                const auto map = point.toMap(); upper.insert(map.value("time").toLongLong(), map.value("value"));
            }
            QVariantList segments, pairs;
            QVariantMap previous;
            bool connected = false;
            for (const auto &value : lower) {
                const auto point = value.toMap();
                const auto time = point.value("time").toLongLong();
                const auto high = upper.constFind(time);
                if (high == upper.cend() || !number(*high) || !number(point.value("value"))) { connected = false; continue; }
                const QVariantMap pair{{"time", time}, {"lower", point.value("value")}, {"upper", *high}};
                pairs.append(pair);
                if (connected && time > previous.value("time").toLongLong()) {
                    bool gap = false;
                    for (auto it = upper.upperBound(previous.value("time").toLongLong()); it != upper.end() && it.key() < time; ++it)
                        if (!number(it.value())) { gap = true; break; }
                    if (!gap) segments.append(QVariantMap{{"from", previous}, {"to", pair}});
                }
                previous = pair; connected = true;
            }
            field["rangeSegments"] = segments;
            field["rangePairs"] = pairs;
        }
        result.append(field);
    }
    const auto events = chartEvents(result);
    const bool types = std::any_of(result.cbegin(),result.cend(),[](const QVariant &entry) {
        return entry.toMap().value("kind") == "type";
    });
    for (auto &entry : result) {
        auto field = entry.toMap();
        const auto kind = field.value("kind", field.value("render")).toString();
        // One decision shared by the legend and both drawing backends.
        field["usesPrecipitationColors"] = types && field.value("precipitationColors",true).toBool()
            && QStringList{"pcpttl_aver","pcpttl_max"}.contains(field.value("key").toString())
            && QStringList{"bars","barsBack","stems","rightBars"}.contains(kind);
        if (kind == "storm" || kind == "flash") field["events"] = events;
        entry = field;
    }
    return result;
}
