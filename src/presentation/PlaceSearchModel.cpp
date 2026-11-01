#include "PlaceSearchModel.hpp"
#include "core/Locations.hpp"
#include <QUrlQuery>

PlaceSearchModel::PlaceSearchModel(QObject *parent, QUrl endpoint)
    : QObject(parent), m_client(nullptr,std::move(endpoint))
{
    m_delay.setSingleShot(true);
    m_delay.setInterval(350);
    connect(&m_delay, &QTimer::timeout, this, &PlaceSearchModel::search);
    connect(&m_client, &Meteo::GeocodingClient::received, this, [this](const QVariantList &places) {
        m_results = places;
        m_busy = false;
        m_error.clear();
        emit stateChanged();
    });
    connect(&m_client, &Meteo::GeocodingClient::failed, this, [this](const QString &message) {
        m_busy = false;
        m_error = message;
        emit stateChanged();
    });
}
void PlaceSearchModel::setQuery(const QString &value)
{
    if (m_query == value) return;
    m_query = value;
    schedule();
}
void PlaceSearchModel::setPolandOnly(bool value)
{
    if (m_polandOnly == value) return;
    m_polandOnly = value;
    schedule();
}
void PlaceSearchModel::schedule()
{
    m_client.cancel();
    m_delay.stop();
    m_results.clear();
    m_error.clear();
    m_busy = m_query.trimmed().size() >= 2;
    if (m_busy) m_delay.start();
    emit queryChanged();
    emit stateChanged();
}
void PlaceSearchModel::search()
{
    m_delay.stop();
    if (m_query.trimmed().size() < 2) return;
    m_busy = true;
    emit stateChanged();
    m_client.search(m_query,m_polandOnly);
}
