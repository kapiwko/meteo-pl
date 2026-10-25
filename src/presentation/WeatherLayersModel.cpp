#include "WeatherLayersModel.hpp"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QUrlQuery>
#include <cmath>
#include <algorithm>

WeatherLayersModel::WeatherLayersModel(QObject *parent, QUrl endpoint) : QObject(parent), m_endpoint(std::move(endpoint))
{
    connect(&m_request, &Meteo::JsonRequest::received, this, [this](const QJsonDocument &document) {
        m_runs.clear();
        for (const auto &entry : document.object().value("dates").toArray()) {
            const auto object = entry.toObject();
            const auto compact = object.value("date").toString();
            const auto iso = compact.left(4) + '-' + compact.mid(4, 2) + '-' + compact.mid(6, 2) + compact.mid(8);
            const auto date = QDateTime::fromString(iso, Qt::ISODate);
            if (!date.isValid()) continue;
            m_runs.append(QVariantMap{{"time", date.toSecsSinceEpoch()}, {"iso", date.toUTC().toString(Qt::ISODate)},
                                      {"label", QLocale().toString(date.toLocalTime(), QStringLiteral("ddd d MMM, HH:mm"))},
                                      {"firstStep", object.value("firstStep").toInt(1)}, {"lastStep", object.value("lastStep").toInt(60)}});
        }
        std::sort(m_runs.begin(), m_runs.end(), [](const QVariant &a, const QVariant &b) { return a.toMap()["time"].toLongLong() > b.toMap()["time"].toLongLong(); });
        m_runIndex = 0;
        m_step = std::clamp(1, firstStep(), lastStep());
        m_busy = false;
        m_error = m_runs.isEmpty() ? QStringLiteral("Brak opublikowanych map wybranego modelu.") : QString();
        publish();
    });
    connect(&m_request, &Meteo::JsonRequest::failed, this, [this](const QString &error) { m_error = error; m_busy = false; publish(); });
}
QVariantList WeatherLayersModel::layers() const
{
    if (m_model == "gfs") return {
        QVariantMap{{"id", "gfs:GFS_TMP_2-HTGL"}, {"label", "Temperatura na 2 m"}},
        QVariantMap{{"id", "gfs:GFS_TMP_0-SFC"}, {"label", "Temperatura gruntu"}},
        QVariantMap{{"id", "gfs:GFS_pressure"}, {"label", "Ciśnienie"}},
        QVariantMap{{"id", "gfs:GFS_cloud"}, {"label", "Chmury i opady"}}};
    return {
        QVariantMap{{"id", "um:UM4_TEMPERATURE_AT_1_5M"}, {"label", "Temperatura na 1,5 m"}},
        QVariantMap{{"id", "um:UM4_SURFACE_TEMP_AFTER_TIMESTEP"}, {"label", "Temperatura gruntu"}},
        QVariantMap{{"id", "um:UM4_PRESSURE_AT_MEAN_SEA_LEVEL"}, {"label", "Ciśnienie"}},
        QVariantMap{{"id", "um:UM4_WIND"}, {"label", "Wiatr na 10 m"}},
        QVariantMap{{"id", "um:UM4_wind_vect"}, {"label", "Wiatr i porywy"}},
        QVariantMap{{"id", "um:UM4_CLOUD"}, {"label", "Chmury i opady"}}};
}
void WeatherLayersModel::publish() { ++m_revision; emit changed(); }
void WeatherLayersModel::setModel(const QString &value)
{
    if (m_model == value) return;
    m_model = value; m_layerIndex = 0; m_runs.clear(); m_request.cancel();
    if (m_active) reload(); else publish();
}
void WeatherLayersModel::setActive(bool value)
{
    if (m_active == value) return;
    m_active = value;
    if (value && m_runs.isEmpty()) reload(); else publish();
}
void WeatherLayersModel::reload()
{
    m_busy = true; m_error.clear(); publish();
    m_request.get(m_endpoint.resolved(QUrl(QStringLiteral("/json/") + (m_model == "gfs" ? "GFS" : "UM4") + QStringLiteral("_min_max.json"))));
}
void WeatherLayersModel::setLayerIndex(int value) { m_layerIndex = std::clamp(value, 0, int(layers().size()) - 1); publish(); }
void WeatherLayersModel::setRunIndex(int value) { m_runIndex = std::clamp(value, 0, std::max(0, int(m_runs.size()) - 1)); m_step = std::clamp(m_step, firstStep(), lastStep()); publish(); }
void WeatherLayersModel::setStep(int value) { const auto step = std::clamp(value, firstStep(), lastStep()); if (m_step == step) return; m_step = step; publish(); }
int WeatherLayersModel::firstStep() const { return m_runs.isEmpty() ? 1 : m_runs.at(m_runIndex).toMap()["firstStep"].toInt(); }
int WeatherLayersModel::lastStep() const { return m_runs.isEmpty() ? 1 : m_runs.at(m_runIndex).toMap()["lastStep"].toInt(); }
QString WeatherLayersModel::layerId() const { return layers().at(m_layerIndex).toMap()["id"].toString(); }
QString WeatherLayersModel::timeLabel() const
{
    if (m_runs.isEmpty()) return {};
    const auto timestamp = m_runs.at(m_runIndex).toMap()["time"].toLongLong() + m_step * 3600;
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(timestamp), QStringLiteral("ddd d MMM · HH:mm t"));
}
QUrl WeatherLayersModel::tileUrl(int x, int y, int zoom) const
{
    return tileUrlAtStep(x, y, zoom, m_step);
}
QUrl WeatherLayersModel::tileUrlAtStep(int x, int y, int zoom, int step) const
{
    if (m_runs.isEmpty() || zoom < 0 || zoom > 20) return {};
    constexpr double edge = 20037508.342789244;
    const double tileSize = 2 * edge / std::pow(2, zoom);
    const double left = -edge + x * tileSize, top = edge - y * tileSize;
    QUrl url(QStringLiteral("https://mapy.meteo.pl/geoserver/gwc/service/wms"));
    QUrlQuery query;
    const QList<QPair<QString, QString>> items{{"service", "WMS"}, {"request", "GetMap"}, {"version", "1.1.1"},
        {"styles", ""}, {"format", "image/png"}, {"transparent", "true"}, {"tiled", "true"}, {"width", "256"},
        {"height", "256"}, {"srs", "EPSG:3857"}, {"layers", layerId()}, {"TIME", m_runs.at(m_runIndex).toMap()["iso"].toString()},
        {"DIM_FORECAST", QString::number(std::clamp(step, firstStep(), lastStep()))}, {"client", "app"},
        {"bbox", QStringLiteral("%1,%2,%3,%4").arg(left, 0, 'f', 6).arg(top - tileSize, 0, 'f', 6).arg(left + tileSize, 0, 'f', 6).arg(top, 0, 'f', 6)}};
    for (const auto &item : items) query.addQueryItem(item.first, item.second);
    url.setQuery(query);
    return url;
}
QUrl WeatherLayersModel::legendUrl() const
{
    QUrl url(QStringLiteral("https://mapy.meteo.pl/geoserver/wms"));
    QUrlQuery query;
    query.addQueryItem("service", "WMS"); query.addQueryItem("request", "GetLegendGraphic");
    query.addQueryItem("format", "image/png"); query.addQueryItem("layer", layerId());
    url.setQuery(query); return url;
}
