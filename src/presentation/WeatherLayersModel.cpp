#include "core/Localization.hpp"
#include "WeatherLayersModel.hpp"
#include "core/Forecast.hpp"
#include <QDateTime>
#include <QLocale>
#include <algorithm>

WeatherLayersModel::WeatherLayersModel(QObject *parent, QUrl endpoint) : QObject(parent), m_source(std::move(endpoint))
{
    connect(&m_request, &Meteo::JsonRequest::received, this, [this](const QJsonDocument &document) {
        const auto parsed = Meteo::MeteoMapSource::parseRuns(document,m_model);
        m_busy = false;
        if (!parsed) {
            m_error = Meteo::i18n("Invalid meteo.pl response format.");
            publish();
            return;
        }
        m_publishedRuns = *parsed;
        m_runs.clear();
        for (const auto &run : m_publishedRuns) {
            m_runs.append(QVariantMap{{"time",run.date.toSecsSinceEpoch()},{"iso",run.date.toString(Qt::ISODate)},
                {"label",QLocale().toString(run.date.toLocalTime(),QStringLiteral("ddd d MMM, HH:mm"))},
                {"firstStep",run.firstStep},{"lastStep",run.lastStep},{"stepHours",run.stepHours},{"sourceRevision",run.sourceRevision}});
        }
        m_runIndex = 0;
        m_step = m_publishedRuns.isEmpty() ? 1 : m_publishedRuns.first().boundedStep(1);
        m_error = m_runs.isEmpty() ? Meteo::i18n("No maps have been published for this model.") : QString();
        publish();
    });
    connect(&m_request, &Meteo::JsonRequest::failed, this, [this](const QString &error) { m_error = error; m_busy = false; publish(); });
}
QVariantList WeatherLayersModel::layers() const
{
    if (m_model == "gfs") return {
        QVariantMap{{"id", "gfs:GFS_TMP_2-HTGL"}, {"label", Meteo::i18n("Temperature at 2 m")}},
        QVariantMap{{"id", "gfs:GFS_TMP_0-SFC"}, {"label", Meteo::i18n("Ground temperature")}},
        QVariantMap{{"id", "gfs:GFS_pressure"}, {"label", Meteo::i18n("Pressure")}},
        QVariantMap{{"id", "gfs:GFS_cloud"}, {"label", Meteo::i18n("Clouds and precipitation")}}};
    return {
        QVariantMap{{"id", "um:UM4_TEMPERATURE_AT_1_5M"}, {"label", Meteo::i18n("Temperature at 1.5 m")}},
        QVariantMap{{"id", "um:UM4_SURFACE_TEMP_AFTER_TIMESTEP"}, {"label", Meteo::i18n("Ground temperature")}},
        QVariantMap{{"id", "um:UM4_PRESSURE_AT_MEAN_SEA_LEVEL"}, {"label", Meteo::i18n("Pressure")}},
        QVariantMap{{"id", "um:UM4_WIND"}, {"label", Meteo::i18n("Wind at 10 m")}},
        QVariantMap{{"id", "um:UM4_wind_vect"}, {"label", Meteo::i18n("Wind and gusts")}},
        QVariantMap{{"id", "um:UM4_CLOUD"}, {"label", Meteo::i18n("Clouds and precipitation")}}};
}
void WeatherLayersModel::publish() { ++m_revision; emit changed(); }
void WeatherLayersModel::setModel(const QString &value)
{
    if (!Meteo::knownModel(value) || m_model == value) return;
    m_model = value; m_layerIndex = 0; m_runs.clear(); m_publishedRuns.clear(); m_busy = false; m_request.cancel();
    if (m_active) reload(false); else publish();
}
void WeatherLayersModel::setActive(bool value)
{
    if (m_active == value) return;
    m_active = value;
    if (value && m_runs.isEmpty()) reload(false); else publish();
}
void WeatherLayersModel::reload(bool force)
{
    m_busy = true; m_error.clear(); publish();
    const auto model=m_model;
    m_request.get(m_source.runsUrl(model), 300, force, [model](const QJsonDocument &document) {
        return Meteo::MeteoMapSource::parseRuns(document,model).has_value();
    });
}
void WeatherLayersModel::setLayerIndex(int value) { m_layerIndex = std::clamp(value, 0, int(layers().size()) - 1); publish(); }
void WeatherLayersModel::setRunIndex(int value) { m_runIndex = std::clamp(value, 0, std::max(0, int(m_runs.size()) - 1)); m_step = m_publishedRuns.isEmpty() ? 1 : m_publishedRuns[m_runIndex].boundedStep(m_step); publish(); }
void WeatherLayersModel::setStep(int value) { const auto step = m_publishedRuns.isEmpty() ? 1 : m_publishedRuns[m_runIndex].boundedStep(value); if (m_step == step) return; m_step = step; publish(); }
int WeatherLayersModel::firstStep() const { return m_publishedRuns.isEmpty() ? 1 : m_publishedRuns[m_runIndex].firstStep; }
int WeatherLayersModel::lastStep() const { return m_publishedRuns.isEmpty() ? 1 : m_publishedRuns[m_runIndex].lastStep; }
int WeatherLayersModel::stepHours() const { return m_publishedRuns.isEmpty() ? 1 : m_publishedRuns[m_runIndex].stepHours; }
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
    if (m_publishedRuns.isEmpty()) return {};
    return m_source.tileUrl(layerId(),m_publishedRuns[m_runIndex],step,x,y,zoom);
}
QUrl WeatherLayersModel::legendUrl() const
{
    return m_source.legendUrl(layerId());
}
