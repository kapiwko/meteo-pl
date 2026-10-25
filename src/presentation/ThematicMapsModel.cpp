#include "ThematicMapsModel.hpp"
#include "core/Localization.hpp"
#include <QDateTime>
#include <algorithm>
#include <cmath>

ThematicMapsModel::ThematicMapsModel(QObject *parent, QUrl endpoint, Clock clock)
    : QObject(parent), m_source(std::move(endpoint)),
      m_clock(clock ? std::move(clock) : Clock([] { return QDateTime::currentSecsSinceEpoch(); }))
{
    latest();
}
QVariantList ThematicMapsModel::catalog() const
{
    QVariantList result;
    for (const auto &layer : Meteo::ThematicMapSource::layers())
        result.append(QVariantMap{{"name", layer.name}, {"id", layer.id}, {"model", layer.model}});
    return result;
}
void ThematicMapsModel::setEndpoint(const QUrl &url)
{
    if (url==endpoint() || !url.isValid() || url.host().isEmpty() || (url.scheme()!="http" && url.scheme()!="https")) return;
    m_source=Meteo::ThematicMapSource(url);
    m_error.clear();
    emit changed();
}
void ThematicMapsModel::setLayerIndex(int value)
{
    value = std::clamp(value, 0, int(Meteo::ThematicMapSource::layers().size()) - 1);
    if (value == m_layerIndex)
        return;
    const auto wasWave = wave();
    m_layerIndex = value;
    if (wave() != wasWave) {
        m_run = Meteo::ThematicMapSource::latestRun(value, m_clock());
        m_step = 12;
    }
    m_attempts = 0;
    m_error.clear();
    emit changed();
}
void ThematicMapsModel::setStep(int value)
{
    value = Meteo::ThematicMapSource::boundedStep(m_layerIndex, value);
    if (value == m_step)
        return;
    m_step = value;
    m_error.clear();
    emit changed();
}
void ThematicMapsModel::setRun(double value)
{
    if (!std::isfinite(value) || value < 0 || value > 253402300799.0)
        return;
    const qint64 period = Meteo::ThematicMapSource::layer(m_layerIndex).runPeriodHours() * 3600;
    const auto run = qint64(value) / period * period;
    if (m_run == run)
        return;
    m_run = run;
    m_attempts = 4;
    m_error.clear();
    emit changed();
}
bool ThematicMapsModel::canGoLater() const
{
    return m_run > 0 && m_run + Meteo::ThematicMapSource::layer(m_layerIndex).runPeriodHours() * 3600 <=
                            Meteo::ThematicMapSource::latestRun(m_layerIndex, m_clock());
}
QUrl ThematicMapsModel::urlForStep(int hour) const { return m_source.imageUrl(m_layerIndex, m_run, hour); }
int ThematicMapsModel::nextStep() const
{
    return m_step + stepHours() > lastStep() ? stepHours() : m_step + stepHours();
}
QVariantList ThematicMapsModel::animationUrls() const
{
    QVariantList urls;
    if (m_run <= 0)
        return urls;
    for (int hour = stepHours(); hour <= lastStep(); hour += stepHours())
        urls.append(urlForStep(hour));
    return urls;
}
void ThematicMapsModel::latest()
{
    m_attempts = 0;
    m_error.clear();
    m_run = Meteo::ThematicMapSource::latestRun(m_layerIndex, m_clock());
    emit changed();
    emit reloadRequested();
}
void ThematicMapsModel::earlier()
{
    if (m_run <= 0)
        return;
    setRun(std::max<qint64>(0, m_run - Meteo::ThematicMapSource::layer(m_layerIndex).runPeriodHours() * 3600));
}
void ThematicMapsModel::later()
{
    if (canGoLater())
        setRun(m_run + Meteo::ThematicMapSource::layer(m_layerIndex).runPeriodHours() * 3600);
}
void ThematicMapsModel::imageFailed(const QUrl &url, const QString &error, int status)
{
    if (url != imageUrl())
        return;
    if (status == 404 && m_attempts < 4 &&
        m_run > Meteo::ThematicMapSource::layer(m_layerIndex).runPeriodHours() * 3600) {
        ++m_attempts;
        m_run -= Meteo::ThematicMapSource::layer(m_layerIndex).runPeriodHours() * 3600;
        m_error.clear();
    } else
        m_error = status == 404
                      ? Meteo::i18n("No map is available for this run and step. Choose an earlier run or another hour.")
                      : error;
    emit changed();
}
void ThematicMapsModel::imageReady(const QUrl &url)
{
    if (url != imageUrl() || m_error.isEmpty())
        return;
    m_error.clear();
    emit changed();
}
