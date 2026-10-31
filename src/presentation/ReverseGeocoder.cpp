#include "ReverseGeocoder.hpp"
#include "core/Localization.hpp"
#include "core/Forecast.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>

ReverseGeocoder::ReverseGeocoder(QObject *parent, QUrl endpoint) : QObject(parent), m_endpoint(std::move(endpoint))
{
    connect(&m_request, &Meteo::JsonRequest::received, this, [this](const QJsonDocument &document) {
        m_busy = false;
        const auto name = placeName(document);
        m_error = name.isEmpty() ? Meteo::i18n("Could not find the place name. You can enter a name yourself.") : QString{};
        emit stateChanged();
        if (!name.isEmpty()) emit resolved(name);
    });
    connect(&m_request, &Meteo::JsonRequest::failed, this, [this](const QString &) {
        m_busy = false;
        m_error = Meteo::i18n("Could not find the place name. You can enter a name yourself.");
        emit stateChanged();
    });
}
QString ReverseGeocoder::placeName(const QJsonDocument &document)
{
    const auto features = document.object().value("features").toArray();
    if (features.isEmpty()) return {};
    const auto properties = features.first().toObject().value("properties").toObject();
    for (const auto key : {"city", "town", "village", "municipality", "name"}) {
        const auto value = properties.value(key).toString().trimmed();
        if (!value.isEmpty()) return value.left(100);
    }
    return {};
}
void ReverseGeocoder::lookup(double latitude, double longitude)
{
    cancel();
    if (!Meteo::validLocation(latitude, longitude, QStringLiteral("gfs"))) return;
    QUrl url(m_endpoint);
    QUrlQuery query;
    query.addQueryItem("lat", QString::number(latitude, 'f', 6));
    query.addQueryItem("lon", QString::number(longitude, 'f', 6));
    query.addQueryItem("limit", "1");
    url.setQuery(query);
    m_busy = true;
    emit stateChanged();
    m_request.get(url, 24 * 3600);
}
void ReverseGeocoder::cancel()
{
    m_request.cancel();
    m_busy = false;
    m_error.clear();
    emit stateChanged();
}
