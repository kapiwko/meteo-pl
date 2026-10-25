#include "MeteoMapSource.hpp"
#include "core/Forecast.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>

namespace Meteo {
int MapRun::boundedStep(int step) const {
    if (firstStep < 0 || lastStep < firstStep || stepHours <= 0)
        return 0;
    return firstStep + (std::clamp(step, firstStep, lastStep) - firstStep) / stepHours * stepHours;
}
MeteoMapSource::MeteoMapSource(QUrl endpoint) : m_endpoint(std::move(endpoint)) {}
QUrl MeteoMapSource::runsUrl(const QString &model) const {
    if (!knownModel(model))
        return {};
    return m_endpoint.resolved(
        QUrl(QStringLiteral("/json/") + (model == "gfs" ? "GFS" : "UM4") + QStringLiteral("_min_max.json")));
}
std::optional<QVector<MapRun>> MeteoMapSource::parseRuns(const QJsonDocument &document, const QString &model) {
    if (!knownModel(model) || !document.isObject() || !document.object().value("dates").isArray())
        return std::nullopt;
    const auto entries = document.object().value("dates").toArray();
    if (entries.size() > 1024)
        return std::nullopt;
    QMap<qint64, MapRun> unique;
    for (const auto &entry : entries) {
        if (!entry.isObject())
            continue;
        const auto object = entry.toObject();
        auto iso = object.value("date").toString();
        if (iso.size() > 8 && iso[8] == 'T')
            iso = iso.left(4) + '-' + iso.mid(4, 2) + '-' + iso.mid(6, 2) + iso.mid(8);
        const auto date = QDateTime::fromString(iso, Qt::ISODate).toUTC();
        const int first = object.value("firstStep").toInt(-1), last = object.value("lastStep").toInt(-1);
        const int hours = object.value("stepHours").toInt(model == "gfs" ? 3 : 1);
        if (!date.isValid() || date.toSecsSinceEpoch() <= 0 || date.toSecsSinceEpoch() > 253402300799LL || first < 0 ||
            last < first || last > 1000 || hours < 1 || hours > 24 || (last - first) % hours)
            continue;
        unique.insert(
            date.toSecsSinceEpoch(),
            {date, first, last, hours,
             QStringLiteral("%1:%2:%3:%4:%5").arg(model).arg(date.toSecsSinceEpoch()).arg(first).arg(last).arg(hours)});
    }
    if (!entries.isEmpty() && unique.isEmpty())
        return std::nullopt;
    QVector<MapRun> runs;
    for (auto it = unique.cend(); it != unique.cbegin();) {
        --it;
        runs.append(it.value());
    }
    return runs;
}
QUrl MeteoMapSource::tileUrl(const QString &layer, const MapRun &run, int step, int x, int y, int zoom) const {
    if (layer.isEmpty() || !run.date.isValid() || run.firstStep < 0 || run.lastStep < run.firstStep ||
        run.stepHours <= 0 || zoom < 0 || zoom > 20)
        return {};
    const qint64 count = qint64(1) << zoom;
    if (y < 0 || y >= count)
        return {};
    const auto wrapped = (qint64(x) % count + count) % count;
    constexpr double edge = 20037508.342789244;
    const double size = 2 * edge / count, left = -edge + wrapped * size, top = edge - y * size;
    QUrl url = m_endpoint.resolved(QUrl(QStringLiteral("/geoserver/gwc/service/wms")));
    QUrlQuery query;
    const QList<QPair<QString, QString>> values{{"service", "WMS"},
                                                {"request", "GetMap"},
                                                {"version", "1.1.1"},
                                                {"styles", ""},
                                                {"format", "image/png"},
                                                {"transparent", "true"},
                                                {"tiled", "true"},
                                                {"width", "256"},
                                                {"height", "256"},
                                                {"srs", "EPSG:3857"},
                                                {"layers", layer},
                                                {"TIME", run.date.toUTC().toString(Qt::ISODate)},
                                                {"DIM_FORECAST", QString::number(run.boundedStep(step))},
                                                {"client", "app"},
                                                {"bbox", QStringLiteral("%1,%2,%3,%4")
                                                             .arg(left, 0, 'f', 6)
                                                             .arg(top - size, 0, 'f', 6)
                                                             .arg(left + size, 0, 'f', 6)
                                                             .arg(top, 0, 'f', 6)}};
    for (const auto &value : values)
        query.addQueryItem(value.first, value.second);
    url.setQuery(query);
    return url;
}
QUrl MeteoMapSource::legendUrl(const QString &layer) const {
    if (layer.isEmpty())
        return {};
    QUrl url = m_endpoint.resolved(QUrl(QStringLiteral("/geoserver/wms")));
    QUrlQuery query;
    query.addQueryItem("service", "WMS");
    query.addQueryItem("request", "GetLegendGraphic");
    query.addQueryItem("format", "image/png");
    query.addQueryItem("layer", layer);
    url.setQuery(query);
    return url;
}
} // namespace Meteo
