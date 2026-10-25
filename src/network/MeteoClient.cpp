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
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}
} // namespace

MeteoClient::MeteoClient(QObject *parent, QUrl endpoint) : QObject(parent), m_endpoint(std::move(endpoint)) {}

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
    // Bound buffering even if Content-Length is missing or incorrect.
    connect(reply, &QIODevice::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 2 * 1024 * 1024)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, consume] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        m_reply = nullptr;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            emit failed(QStringLiteral("Nie udało się pobrać prognozy z meteo.pl (HTTP %1). %2")
                            .arg(status).arg(reply->errorString()));
            return;
        }
        const auto body = reply->readAll();
        if (body.size() > 2 * 1024 * 1024) {
            emit failed(QStringLiteral("Odpowiedź serwera jest zbyt duża."));
            return;
        }
        consume(body);
    });
}

void MeteoClient::fetch(double latitude, double longitude, const QString &model, qint64 requestedRun)
{
    cancel();
    if (!validLocation(latitude, longitude, model)) {
        emit failed(QStringLiteral("Lokalizacja jest poza obszarem modelu. Dla punktów poza Europą Środkową wybierz GFS."));
        return;
    }
    const auto generation = m_generation;
    auto request = requestFor(m_endpoint.resolved(QUrl(QStringLiteral("/meteorograms/available"))));
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
            emit failed(QStringLiteral("Serwer nie udostępnia aktualnego uruchomienia wybranego modelu."));
            return;
        }
        const QJsonObject point{{"lat", QString::number(latitude, 'f', 6)}, {"lon", QString::number(longitude, 'f', 6)}};
        const QJsonObject payload{{"point", point}, {"date", *run}};
        auto forecastRequest = requestFor(m_endpoint.resolved(QUrl(QStringLiteral("/meteorograms/") + model)));
        forecastRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        readReply(m_network.post(forecastRequest, QJsonDocument(payload).toJson(QJsonDocument::Compact)), generation,
                  [this](const QByteArray &json) { emit received(json); });
    });
}
} // namespace Meteo
