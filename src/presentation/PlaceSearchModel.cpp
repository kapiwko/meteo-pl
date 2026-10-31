#include "PlaceSearchModel.hpp"
#include "core/Locations.hpp"
#include <QUrlQuery>

PlaceSearchModel::PlaceSearchModel(QObject *parent, QUrl endpoint)
    : QObject(parent), m_endpoint(std::move(endpoint))
{
    m_delay.setSingleShot(true);
    m_delay.setInterval(350);
    connect(&m_delay, &QTimer::timeout, this, &PlaceSearchModel::search);
    connect(&m_request, &Meteo::JsonRequest::received, this, [this](const QJsonDocument &document) {
        m_results = Meteo::parseLocations(document);
        m_busy = false;
        m_error.clear();
        emit stateChanged();
    });
    connect(&m_request, &Meteo::JsonRequest::failed, this, [this](const QString &message) {
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
    m_request.cancel();
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
    QUrl url(m_endpoint);
    QUrlQuery query;
    // Photon supports incomplete names; the legacy geocoder only found full
    // names. Keep the same autocomplete service when restricting the country.
    query.addQueryItem(QStringLiteral("q"), m_query.trimmed());
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("20"));
    query.addQueryItem(QStringLiteral("osm_tag"), QStringLiteral("place"));
    query.addQueryItem(QStringLiteral("lang"), QStringLiteral("en"));
    if (m_polandOnly) query.addQueryItem(QStringLiteral("countrycode"), QStringLiteral("PL"));
    url.setQuery(query);
    m_busy = true;
    emit stateChanged();
    m_request.get(url, 24 * 3600);
}
