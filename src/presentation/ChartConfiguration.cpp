#include "core/Localization.hpp"
#include "ChartConfiguration.hpp"
#include "ChartAnalysis.hpp"
#include "ForecastFields.hpp"
#include <QColor>
#include <QSet>
#include <cmath>
#include <algorithm>

QString seriesScale(const QString &key)
{
    for (const auto &field : forecastFields()) if (field.key == key) {
        if (key == "fog_max") return "fraction";
        if (key == "pcpttlprob_point") return "probability";
        if (field.group == "ground") return "temperature";
        return field.group;
    }
    return "linear";
}

QVariantMap defaultSeriesStyle(const QString &key)
{
    for (const auto &field : forecastFields()) if (field.key == key) {
        QString render = "line", lower, fill = "solid";
        int layer = 15;
        if (key == "airtmp_max" || key == "wchill_max" || key == "grdtmp_max") {
            lower = key; lower.replace("_max", "_min");
            render = key == "grdtmp_max" ? "rangeStems" : "range";
            layer = key == "wchill_max" ? 0 : key == "airtmp_max" ? 1 : 15;
        }
        if (key == "wchill_point") layer = 12;
        if (key == "airtmp_point") layer = 13;
        if (key == "dwptmp_point") { render = "points"; layer = 14; }
        if (key == "pcpttl_aver") { render = "bars"; layer = 10; }
        if (key == "pcpttl_max") { render = "stems"; layer = 11; }
        if (field.kind == "type") render = "type";
        if (field.kind == "direction") render = "direction";
        if (key == "wind_gust_max") { render = "gusts"; layer = 16; }
        if (key == "slpres_point") { render = "area"; fill = "pressure"; }
        if (key == "wind10_sd_true_prev_point") { render = "area"; fill = "wind"; }
        if (key.startsWith("cldbse") || key == "cldtop") {
            render = "cloudPoint";
            layer = key == "cldbse79" ? 1 : key == "cldbse65" ? 2 : key == "cldbse45" ? 3 : key == "cldbse25" ? 4 : key == "cldbse01" ? 5 : 6;
        }
        const QStringList clouds{"cldhigh_aver", "cldmed_aver", "cldlow_aver", "cldvlow_aver"};
        if (clouds.contains(key)) { render = "cloudArea"; layer = clouds.indexOf(key); }
        if (key == "fog_max") { render = "rightBars"; layer = 20; }
        if (key == "storm_max") render = "storm";
        if (key == "flash_max") render = "flash";
        QString label = field.label;
        if (!lower.isEmpty()) label = key == "grdtmp_max" ? Meteo::i18n("Ground temperature min–max") : key == "wchill_max" ? Meteo::i18n("Feels like min–max") : Meteo::i18n("Air temperature min–max");
        return {{"key", key}, {"label", label}, {"unit", field.unit}, {"enabled", true},
            {"render", render}, {"lowerKey", lower}, {"axis", "auto"}, {"color", field.color},
            {"darkColor", forecastDarkColor(field.color, key)}, {"lineWidth", render == "stems" ? 2.5 : render == "rangeStems" || render == "cloudArea" ? 1.25 : 2.0},
            {"lineStyle", "solid"}, {"opacity", render == "range" ? 191.0 / 255 : 1.0}, {"fill", fill}, {"layer", layer},
            {"precipitationColors", key == "pcpttl_aver" || key == "pcpttl_max"},
            {"eventColors", true}, {"pointSize", key == "dwptmp_point" ? 2.0 : key == "storm_max" || key == "flash_max" ? 4.0 : 3.0}};
    }
    return {};
}

QVariantList chartSeriesCatalog()
{
    QVariantList result;
    for (const auto &field : forecastFields()) result.append(defaultSeriesStyle(field.key));
    return result;
}

QVariantMap normalizeSeriesStyle(const QVariantMap &value)
{
    auto result = defaultSeriesStyle(value["key"].toString());
    if (result.isEmpty()) return {};
    const QStringList renderers{"line", "points", "area", "bars", "stems", "gusts", "cloudArea", "cloudPoint", "rightBars", "direction", "storm", "flash", "type", "range", "rangeStems"};
    if (value.contains("enabled")) result["enabled"] = value["enabled"].toBool();
    if (value.contains("precipitationColors")) result["precipitationColors"] = value["precipitationColors"].toBool();
    if (value.contains("eventColors")) result["eventColors"] = value["eventColors"].toBool();
    if (renderers.contains(value["render"].toString())) result["render"] = value["render"];
    if (result["lowerKey"].toString().isEmpty() && QStringList{"range", "rangeStems"}.contains(result["render"].toString())) result["render"] = "line";
    if (QStringList{"auto", "left", "right"}.contains(value["axis"].toString())) result["axis"] = value["axis"];
    for (const auto &key : {"color", "darkColor"}) if (value.contains(key) && QColor(value[key].toString()).isValid()) result[key] = QColor(value[key].toString()).name();
    for (const auto &key : {"lineWidth", "opacity"}) if (value.contains(key)) {
        bool valid = false;
        const double number = value[key].toDouble(&valid);
        if (valid && std::isfinite(number)) result[key] = std::clamp(number, key == QLatin1String("lineWidth") ? 0.5 : 0.0, key == QLatin1String("lineWidth") ? 8.0 : 1.0);
    }
    if (value.contains("layer")) result["layer"] = std::clamp(value["layer"].toInt(), 0, 100);
    if (value.contains("pointSize")) {
        bool valid = false; const double size = value["pointSize"].toDouble(&valid);
        if (valid && std::isfinite(size)) result["pointSize"] = std::clamp(size, 1.0, 10.0);
    }
    if (QStringList{"solid", "dashed", "dotted"}.contains(value["lineStyle"].toString())) result["lineStyle"] = value["lineStyle"];
    if (QStringList{"solid", "pressure", "wind"}.contains(value["fill"].toString())) result["fill"] = value["fill"];
    return result;
}

QStringList chartFieldKeys(const QVariantList &series)
{
    QStringList keys;
    for (const auto &value : series) {
        const auto style = value.toMap();
        if (!style["enabled"].toBool()) continue;
        if (QStringList{"range", "rangeStems"}.contains(style["render"].toString()) && !style["lowerKey"].toString().isEmpty()) keys.append(style["lowerKey"].toString());
        keys.append(style["key"].toString());
    }
    keys.removeDuplicates(); return keys;
}

QVariantList defaultChartLayout(const QString &profile)
{
    QStringList enabled{"temperature", "precipitation", "probability", "pressure", "wind", "cloudHeights", "clouds"};
    if (profile == "basic") enabled = {"temperature", "precipitation", "wind", "pressure", "clouds"};
    if (profile == "outdoor") enabled = {"temperature", "precipitation", "probability", "wind", "clouds"};
    QVariantList layout;
    for (const auto &chart : forecastCharts()) {
        QVariantList series;
        for (const auto &key : chart.fields) {
            if (key.endsWith("_min") && chart.fields.contains(QString(key).replace("_min", "_max"))) continue;
            auto style = defaultSeriesStyle(key);
            style["axis"] = (chart.key == "precipitation" && key == "realhum_aver") || (chart.key == "cloudHeights" && key == "visibl_min") || (chart.key == "clouds" && key == "fog_max") || (chart.key == "humidity" && key == "fog_max") ? "right" : "left";
            series.append(style);
        }
        layout.append(QVariantMap{{"key", chart.key}, {"title", chart.title}, {"enabled", chart.key != "windDirection" && (profile == "full" || enabled.contains(chart.key))},
            {"leftScale", chart.key == "ground" ? "temperature" : chart.key == "windDirection" ? "direction" : chart.key},
            {"rightScale", "auto"}, {"series", series}, {"fields", chartFieldKeys(series)}});
    }
    return layout;
}

QVariantList normalizeChartLayout(const QVariantList &saved)
{
    const auto defaults = defaultChartLayout("classic");
    QVariantList result;
    QSet<QString> seen;
    for (const auto &item : saved) {
        auto chart = item.toMap();
        const auto key = chart["key"].toString();
        if (key.isEmpty() || key.size() > 100 || seen.contains(key) || result.size() >= 50) continue;
        QVariantMap standard;
        for (const auto &value : defaults) if (value.toMap()["key"] == key) { standard = value.toMap(); break; }
        if (standard.isEmpty() && !key.startsWith("chart:")) continue;
        QVariantList series;
        QSet<QString> seriesSeen;
        if (chart.contains("series")) {
            for (const auto &entry : chart["series"].toList()) {
                const auto style = normalizeSeriesStyle(entry.toMap());
                if (style.isEmpty() || seriesSeen.contains(style["key"].toString())) continue;
                series.append(style); seriesSeen.insert(style["key"].toString());
            }
        } else {
            const auto fields = chart.value("fields", standard["fields"]).toStringList();
            for (const auto &field : fields) {
                if (field.endsWith("_min") && fields.contains(QString(field).replace("_min", "_max"))) continue;
                auto style = defaultSeriesStyle(field);
                for (const auto &entry : standard["series"].toList()) if (entry.toMap()["key"] == field) style = entry.toMap();
                if (!style.isEmpty() && !seriesSeen.contains(field)) { series.append(style); seriesSeen.insert(field); }
            }
        }
        QVariantMap normalized{{"key", key}, {"title", chart.value("title", standard.value("title", Meteo::i18n("New chart"))).toString().trimmed().left(80)},
            {"enabled", chart["enabled"].toBool()}, {"series", series}, {"fields", chartFieldKeys(series)},
            {"leftScale", chart.value("leftScale", standard.value("leftScale", "auto"))}, {"rightScale", chart.value("rightScale", "auto")}};
        for (const auto &bound : {"leftMin", "leftMax", "rightMin", "rightMax"}) {
            bool valid = false; const double value = chart[bound].toDouble(&valid);
            if (valid && !chart[bound].isNull() && std::isfinite(value)) normalized[bound] = value;
        }
        if (chart["height"].toInt()>0) normalized["height"]=std::clamp(chart["height"].toInt(),64,800);
        for (const auto &axis : {QStringLiteral("left"), QStringLiteral("right")})
            if (normalized.contains(axis + "Min") && normalized.contains(axis + "Max") && !ChartMath::validRange(normalized[axis + "Min"].toDouble(), normalized[axis + "Max"].toDouble())) { normalized.remove(axis + "Min"); normalized.remove(axis + "Max"); }
        result.append(normalized); seen.insert(key);
    }
    return result;
}

QVariantList integrateWindDirection(const QVariantList &saved)
{
    auto result = normalizeChartLayout(saved);
    int speedIndex = -1, directionIndex = -1;
    for (int i=0; i<result.size(); ++i) {
        const auto chart = result[i].toMap();
        if (!chart["enabled"].toBool()) continue;
        if (chart["key"] == "wind") speedIndex = i;
        if (chart["key"] == "windDirection") directionIndex = i;
    }
    if (speedIndex < 0 || directionIndex < 0) return result;
    auto speed = result[speedIndex].toMap(), direction = result[directionIndex].toMap();
    auto series = speed["series"].toList();
    const auto arrows = direction["series"].toList();
    const auto keys = chartFieldKeys(series);
    if (!keys.contains("wind10_sd_true_prev_point") || keys.contains("wind10_dr_deg_true_prev_point")
        || arrows.size()!=1 || arrows[0].toMap()["key"]!="wind10_dr_deg_true_prev_point"
        || arrows[0].toMap()["render"]!="direction") return result;
    series.append(arrows[0]); // Keep the user's arrow color, opacity and width.
    speed["series"] = series; speed["fields"] = chartFieldKeys(series);
    if (speed["title"] == Meteo::i18n("Wind speed and gusts")) speed["title"] = Meteo::i18n("Wind");
    direction["enabled"] = false;
    result[speedIndex] = speed; result[directionIndex] = direction;
    return result;
}
