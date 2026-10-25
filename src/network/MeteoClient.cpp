#include "core/Localization.hpp"
#include "MeteoClient.hpp"
#include "core/Forecast.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <algorithm>

namespace Meteo {
namespace {
QNetworkRequest requestFor(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "MeteoDesktop/0.1 (Qt; personal desktop client)");
    request.setRawHeader("Accept", "application/json");
    return request;
}
} // namespace

MeteoClient::MeteoClient(QObject *parent, QUrl endpoint, RequestPolicy policy)
    : QObject(parent), m_policy(policy), m_endpoint(std::move(endpoint)) {}

void MeteoClient::cancel()
{
    ++m_generation;
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void MeteoClient::readReply(QNetworkReply *reply, quint64 generation, const std::function<void(QByteArray)> &consume)
{
    m_reply = reply;
    m_policy.enforce(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, consume] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        m_reply = nullptr;
        if (RequestPolicy::deadlineExceeded(reply)) {
            emit failed(Meteo::i18n("The request timed out. Try again."));
            return;
        }
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            emit failed(Meteo::i18n("Could not download the forecast from meteo.pl (HTTP %1). %2")
                            .arg(status).arg(reply->errorString()));
            return;
        }
        const auto body = reply->readAll();
        if (body.size() > 2 * 1024 * 1024) {
            emit failed(Meteo::i18n("The server response is too large."));
            return;
        }
        consume(body);
    });
}

void MeteoClient::fetch(double latitude, double longitude, const QString &model, qint64 requestedRun)
{
    cancel();
    if (!validLocation(latitude, longitude, model)) {
        emit failed(Meteo::i18n("This location is outside the model area. Choose GFS for locations outside Central Europe."));
        return;
    }
    const auto generation = m_generation;
    auto request = requestFor(m_endpoint.resolved(QUrl(QStringLiteral("/meteorograms/available"))));
    m_policy.configure(request);
    readReply(m_network.get(request), generation, [this, latitude, longitude, model, generation, requestedRun](const QByteArray &body) {
        QList<qint64> runs;
        for (const auto &value : QJsonDocument::fromJson(body).object().value(model).toArray()) {
            const auto timestamp = value.isString() ? value.toString().toLongLong() : value.toInteger();
            if (timestamp > 0 && !runs.contains(timestamp)) runs.append(timestamp);
        }
        std::sort(runs.begin(), runs.end(), std::greater<>());
        emit runsReceived(runs);
        const auto run = requestedRun > 0 && runs.contains(requestedRun) ? std::optional<qint64>(requestedRun)
            : requestedRun > 0 ? std::nullopt : latestRun(body, model);
        if (!run) {
            emit failed(Meteo::i18n("No current run is available for the selected model."));
            return;
        }
        const QJsonObject point{{"lat", QString::number(latitude, 'f', 6)}, {"lon", QString::number(longitude, 'f', 6)}};
        const QJsonObject payload{{"point", point}, {"date", *run}};
        auto forecastRequest = requestFor(m_endpoint.resolved(QUrl(QStringLiteral("/meteorograms/") + model)));
        m_policy.configure(forecastRequest);
        forecastRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        readReply(m_network.post(forecastRequest, QJsonDocument(payload).toJson(QJsonDocument::Compact)), generation,
                  [this](const QByteArray &json) { emit received(json); });
    });
}
} // namespace Meteo
