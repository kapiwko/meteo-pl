#include "GeocodingClient.hpp"
#include "core/Locations.hpp"
#include <QUrlQuery>
namespace Meteo {
GeocodingClient::GeocodingClient(QObject *parent,QUrl endpoint):QObject(parent),m_endpoint(std::move(endpoint))
{
    connect(&m_request,&JsonRequest::received,this,[this](const QJsonDocument &document) { emit received(parseLocations(document)); });
    connect(&m_request,&JsonRequest::failed,this,&GeocodingClient::failed);
}
void GeocodingClient::search(const QString &text,bool polandOnly)
{
    cancel(); if (text.trimmed().size()<2) { emit received({}); return; }
    QUrl url(m_endpoint); QUrlQuery query;
    query.addQueryItem("q",text.trimmed()); query.addQueryItem("limit","20"); query.addQueryItem("osm_tag","place"); query.addQueryItem("lang","en");
    if (polandOnly) query.addQueryItem("countrycode","PL");
    url.setQuery(query); m_request.get(url,24*3600);
}
}
