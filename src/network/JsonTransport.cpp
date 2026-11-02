#include "JsonTransport.hpp"
#include "HttpCachePolicy.hpp"
#include "core/Localization.hpp"
#include <QNetworkReply>
#include <QJsonArray>
#include <QElapsedTimer>
namespace Meteo {
void JsonTransport::cancel()
{
    ++m_generation;
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}
void JsonTransport::get(const QUrl &url,int cacheSeconds,Validator validator)
{
    cancel(); m_statusCode=0;
    QNetworkRequest request(url);
    m_policy.configure(request);
    request.setRawHeader("User-Agent", "MeteoDesktop/0.2 (Qt desktop client)");
    request.setRawHeader("Accept", "application/json");
    QElapsedTimer elapsed; elapsed.start();
    auto *reply = m_network.get(request);
    m_reply = reply;
    const auto generation = m_generation;
    m_policy.enforce(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, url, cacheSeconds, elapsed, validator=std::move(validator)] {
        reply->deleteLater();
        if (generation != m_generation) return;
        m_reply = nullptr;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_statusCode = status;
        if (RequestPolicy::deadlineExceeded(reply)) {
            emit failed(Meteo::i18n("The request timed out. Try again."));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            emit failed(Meteo::i18n("Could not download data (HTTP %1). %2").arg(status).arg(reply->errorString()));
            return;
        }
        QJsonParseError error;
        const auto body = reply->readAll();
        const auto document = body.trimmed() == "null" ? QJsonDocument(QJsonArray{}) : QJsonDocument::fromJson(body, &error);
        if (error.error != QJsonParseError::NoError || document.isNull()) {
            emit failed(Meteo::i18n("The server returned an invalid JSON document."));
            return;
        }
        if (validator && !validator(document)) {
            emit failed(Meteo::i18n("Invalid meteo.pl response format."));
            return;
        }
        const auto expires=cacheExpiration(reply,QDateTime::currentDateTimeUtc(),cacheSeconds,std::chrono::milliseconds(elapsed.elapsed()));
        emit received(document,expires);
    });
}
}
