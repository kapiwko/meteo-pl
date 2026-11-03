#include "PreparedLegend.hpp"
#include "core/Localization.hpp"
#include <QJsonDocument>
#include <QLocale>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {
QVariantMap colorScale(const QVariantMap &field, const ChartMath::AxisSpec &range, double windScale)
{
    const auto palette = field.value("palette").toList();
    if (palette.isEmpty() || !ChartMath::validRange(range.low, range.high)) return {};
    const double factor = field.value("fill") == "wind" ? windScale : 1;
    const double span = range.high - range.low;
    QVariantList bands;
    double cursor = range.low;
    const auto append = [&](double end, const QVariant &light, const QVariant &dark) {
        bands.append(QVariantMap{{"from", (cursor-range.low)/span}, {"to", (end-range.low)/span},
                                 {"color", light}, {"darkColor", dark}});
        cursor = end;
    };
    for (const auto &entry : palette) {
        const auto stop = entry.toList();
        if (stop.size() != 3 || !std::isfinite(stop[0].toDouble())) continue;
        const double end = std::min(range.high, stop[0].toDouble()*factor);
        if (end > cursor) append(end, stop[1], stop[2]);
        if (cursor >= range.high) break;
    }
    if (cursor < range.high) {
        const auto fallback = field.value("paletteFallback").toMap();
        append(range.high, fallback.value("color"), fallback.value("darkColor"));
    }
    const auto label = [](double value) {
        return QLocale().toString(value, 'f', std::abs(value-std::round(value))<0.0001 ? 0 : 1);
    };
    return {{"key", field["key"]}, {"label", field["label"]}, {"unit", field["unit"]},
            {"bands", bands}, {"minimumLabel", label(range.low)},
            {"middleLabel", label((range.low+range.high)/2)}, {"maximumLabel", label(range.high)},
            {"opacity", field.value("opacity", 1)}};
}
}

QVariantMap prepareChartLegend(const QVariantList &fields, const ChartMath::AxisSpec &left,
                              const ChartMath::AxisSpec &right, double windScale)
{
    QVariantList legend, typeLegend, eventLegend, scales;
    QSet<QByteArray> colorsSeen;
    const auto find = [&](const QString &kind) {
        for (const auto &value : fields) if (value.toMap().value("kind") == kind) return value.toMap();
        return QVariantMap{};
    };
    const auto storm = find("storm"), flash = find("flash");
    for (const auto &value : fields) {
        const auto field = value.toMap();
        const auto kind = field.value("kind").toString();
        QVariantMap scale;
        if (kind == "area" && (field["fill"] == "pressure" || field["fill"] == "wind"))
            scale = colorScale(field, field["axis"] == "right" ? right : left, windScale);
        if (kind != "hidden" && kind != "type" && !(kind == "flash" && !storm.isEmpty())) {
            auto entry = field;
            if (kind == "storm") entry["legendHasLightning"] = !flash.isEmpty();
            if (kind == "storm" && !flash.isEmpty())
                for (const auto &key : {"color", "darkColor", "colors", "eventColors", "pointSize", "lineWidth", "opacity"}) entry[key] = flash[key];
            if (!scale.isEmpty()) entry["legendBands"] = scale["bands"];
            entry["legendLabel"] = (kind == "storm" || kind == "flash")
                ? Meteo::i18n("Storms / lightning") : field["label"];
            for (const auto &key : {"points", "lowerPoints", "rangeSegments", "rangePairs", "events"}) entry.remove(key);
            legend.append(entry);
        }
        if (field.value("usesPrecipitationColors").toBool()) {
            const auto colors = field.value("colors").toMap();
            const QVariantMap rain{{"color", field["color"]}, {"darkColor", field["darkColor"]}};
            const QVariantList applied{rain, colors.value("snow"), colors.value("sleet")};
            const auto identity = QJsonDocument::fromVariant(applied).toJson(QJsonDocument::Compact);
            if (!colorsSeen.contains(identity)) {
                colorsSeen.insert(identity);
                const QStringList labels{Meteo::i18n("Rain"), Meteo::i18n("Snow"), Meteo::i18n("Sleet")};
                for (int i=0; i<3; ++i) {
                    auto color = applied[i].toMap();
                    color["label"] = labels[i]; color["seriesLabel"] = field["label"]; color["key"] = field["key"];
                    color["opacity"] = field.value("opacity", 1); typeLegend.append(color);
                }
            }
        }
        if (!scale.isEmpty()) scales.append(scale);
    }
    if (colorsSeen.size() > 1) for (auto &entry : typeLegend) {
        auto color = entry.toMap(); color["label"] = color["seriesLabel"].toString()+" · "+color["label"].toString(); entry = color;
    }
    if (!storm.isEmpty() || !flash.isEmpty()) {
        // The painter uses the flash series' appearance when both are present.
        const auto style = flash.isEmpty() ? storm : flash;
        const auto colors = style["colors"].toMap();
        const auto outline = colors["outline"].toMap();
        const auto append = [&](const QString &label, const QVariantMap &fill, bool hollow) {
            auto entry = fill;
            entry["label"] = label; entry["hollow"] = hollow;
            entry["outline"] = outline; entry["opacity"] = style.value("opacity", 1);
            entry["key"] = storm.isEmpty() ? flash["key"] : storm["key"];
            eventLegend.append(entry);
        };
        if (!storm.isEmpty()) append(Meteo::i18n("Possible storms"), outline, true);
        if (!flash.isEmpty()) {
            if (style.value("eventColors", true).toBool()) {
                append(Meteo::i18n("Weak storms"), colors["low"].toMap(), false);
                append(Meteo::i18n("Moderate storms"), colors["medium"].toMap(), false);
                append(Meteo::i18n("Strong storms"), colors["high"].toMap(), false);
            } else append(Meteo::i18n("Lightning"), {{"color", style["color"]}, {"darkColor", style["darkColor"]}}, false);
        }
    }
    return {{"legend", legend}, {"typeLegend", typeLegend}, {"eventLegend", eventLegend}, {"colorScales", scales}};
}
