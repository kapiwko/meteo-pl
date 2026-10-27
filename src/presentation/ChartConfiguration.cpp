#include "core/Localization.hpp"
#include "ChartConfiguration.hpp"
#include "ChartAnalysis.hpp"
#include "ForecastFields.hpp"
#include "ProfileFileCodec.hpp"
#include "JsonSchemaValidator.hpp"
#include <QColor>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cmath>
#include <algorithm>

namespace {
const QVariantMap &chartDefaults()
{
    static const auto defaults = bundledChartJson(":/meteo/charts/catalog.json").object().toVariantMap();
    return defaults;
}
}

QString seriesScale(const QString &key)
{
    for (const auto &field : forecastFields()) if (field.key == key) return field.axisScale;
    return "linear";
}

QVariantMap defaultSeriesStyle(const QString &key)
{
    for (const auto &value : chartDefaults()["series"].toList())
        if (value.toMap()["key"] == key) return value.toMap();
    return {};
}

QVariantList chartSeriesCatalog() { return chartDefaults()["series"].toList(); }

QStringList seriesRenderers(const QString &key)
{
    for (const auto &field : forecastFields()) if (field.key == key) return field.renderers;
    return {};
}

QVariantMap normalizeSeriesStyle(const QVariantMap &value)
{
    auto result = defaultSeriesStyle(value["key"].toString());
    if (result.isEmpty()) return {};
    const auto renderers = seriesRenderers(value.value("key").toString());
    if (value.contains("label")) result["label"] = value["label"].toString().trimmed().left(80);
    if (value.contains("lowerKey") && (value["lowerKey"].toString().isEmpty() || !defaultSeriesStyle(value["lowerKey"].toString()).isEmpty()))
        result["lowerKey"] = value["lowerKey"];
    if (value.contains("enabled")) result["enabled"] = value["enabled"].toBool();
    if (value.contains("precipitationColors")) result["precipitationColors"] = value["precipitationColors"].toBool();
    if (value.contains("eventColors")) result["eventColors"] = value["eventColors"].toBool();
    for (const auto &key : {"palette", "paletteFallback"})
        if (value.contains(key)) result[key] = value[key];
    if (value.contains("colors")) {
        auto colors = result["colors"].toMap();
        const auto overrides = value["colors"].toMap();
        for (auto it = overrides.cbegin(); it != overrides.cend(); ++it) colors[it.key()] = it.value();
        result["colors"] = colors;
    }
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

QVariantList defaultChartCatalog()
{
    auto charts = chartDefaults()["charts"].toList();
    for (auto &value : charts) {
        auto chart = value.toMap();
        chart["fields"] = chartFieldKeys(chart["series"].toList());
        value = chart;
    }
    return charts;
}
QVariantMap defaultChartAppearance() { return chartDefaults()["appearance"].toMap(); }
QVariantMap defaultChartPrototype() { return chartDefaults()["newChart"].toMap(); }

QVariantList builtinChartProfiles()
{
    static const QVariantList profiles = [] {
        const auto files = bundledChartJson(":/meteo/profiles/index.json").array();
        if (files.isEmpty()) qFatal("Empty bundled profile index");
        QVariantList result;
        QSet<QString> ids;
        for (const auto &name : files) {
            const auto fileName = name.toString();
            const auto bytes = bundledChartJson(":/meteo/profiles/" + fileName).toJson();
            QString error;
            const auto decoded = ProfileFileCodec::decode(bytes, &error);
            const auto id = QJsonDocument::fromJson(bytes).object()["profiles"].toArray().first().toObject()["id"].toString();
            if (!decoded || decoded->size() != 1 || id.isEmpty() || id == "custom" || id.startsWith("user:")
                || ids.contains(id) || fileName != id + ".json")
                qFatal("Invalid bundled profile %s: %s", qPrintable(fileName), qPrintable(error));
            auto profile = decoded->first().toMap();
            profile["id"] = id;
            const auto revision = QJsonDocument::fromJson(bytes).object()["profiles"].toArray().first().toObject()["revision"].toInt();
            if (revision < 1) qFatal("Missing bundled profile revision: %s", qPrintable(fileName));
            profile["revision"] = revision;
            result.append(profile);
            ids.insert(id);
        }
        return result;
    }();
    return profiles;
}

QVariantList defaultChartLayout(const QString &id)
{
    const auto profiles = builtinChartProfiles();
    for (const auto &entry : profiles)
        if (entry.toMap()["id"] == id) return entry.toMap()["charts"].toList();
    return profiles.first().toMap()["charts"].toList();
}

QVariantList normalizeChartLayout(const QVariantList &saved)
{
    const auto defaults = defaultChartCatalog();
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
        const auto prototype = defaultChartPrototype();
        QVariantMap normalized{{"key", key}, {"title", chart.value("title", standard.value("title", prototype["title"])).toString().trimmed().left(80)},
            {"appearance", chart.value("appearance", standard.value("appearance", defaultChartAppearance()))},
            {"enabled", chart["enabled"].toBool()}, {"series", series}, {"fields", chartFieldKeys(series)},
            {"leftScale", chart.value("leftScale", standard.value("leftScale", prototype["leftScale"]))}, {"rightScale", chart.value("rightScale", prototype["rightScale"])}};
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

QVariantList removeUnusedChartTemplates(const QVariantList &saved)
{
    QHash<QString, QVariantMap> templates;
    for (const auto &entry : normalizeChartLayout(defaultChartCatalog())) {
        auto chart = entry.toMap();
        chart["enabled"] = false;
        templates.insert(chart["key"].toString(), chart);
    }
    auto result = normalizeChartLayout(saved);
    result.removeIf([&](const QVariant &entry) {
        const auto chart = entry.toMap();
        return !chart["enabled"].toBool() && templates.value(chart["key"].toString()) == chart;
    });
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
