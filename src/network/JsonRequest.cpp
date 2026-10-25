#include "JsonRequest.hpp"
#include <QNetworkReply>
#include <QJsonArray>

namespace Meteo {
JsonRequest::JsonRequest(QObject *parent) : QObject(parent) {}
void JsonRequest::cancel()
{
    ++m_generation;
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}
void JsonRequest::get(const QUrl &url)
{
    cancel();
    m_statusCode = 0;
    QNetworkRequest request(url);
    request.setTransferTimeout(15000);
    request.setRawHeader("User-Agent", "MeteoDesktop/0.2 (Qt desktop client)");
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_network.get(request);
    m_reply = reply;
    const auto generation = m_generation;
    connect(reply, &QIODevice::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 4 * 1024 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (generation != m_generation) return;
        m_reply = nullptr;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_statusCode = status;
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            emit failed(QStringLiteral("Nie udało się pobrać danych (HTTP %1). %2").arg(status).arg(reply->errorString()));
            return;
        }
        QJsonParseError error;
        const auto body = reply->readAll();
        const auto document = body.trimmed() == "null" ? QJsonDocument(QJsonArray{}) : QJsonDocument::fromJson(body, &error);
        if (error.error != QJsonParseError::NoError || document.isNull()) {
            emit failed(QStringLiteral("Serwer zwrócił niepoprawny dokument JSON."));
            return;
        }
        emit received(document);
    });
}
}
