#include "LegacyMeteogramModel.hpp"
#include <QNetworkReply>
#include <QUrlQuery>

LegacyMeteogramModel::LegacyMeteogramModel(QObject *parent, QUrl endpoint) : QObject(parent), m_endpoint(std::move(endpoint))
{
    m_delay.setSingleShot(true);
    connect(&m_delay, &QTimer::timeout, this, &LegacyMeteogramModel::reload);
}
void LegacyMeteogramModel::setLookup(const QUrl &url)
{
    if (m_lookup == url) return;
    ++m_generation;
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); m_reply = nullptr; }
    m_lookup = url; m_busy = !url.isEmpty(); m_error.clear();
    m_delay.start(0); emit changed();
}
void LegacyMeteogramModel::reload()
{
    m_delay.stop();
    const auto generation = ++m_generation;
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); m_reply = nullptr; }
    if (m_lookup.isEmpty()) { m_busy = false; emit changed(); return; }
    m_busy = true; m_error.clear(); emit changed();
    QNetworkRequest request(m_lookup);
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    auto *reply = m_network.get(request);
    m_reply = reply;
    QTimer::singleShot(20000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (generation != m_generation) return;
        m_reply = nullptr; m_busy = false;
        const auto redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const QUrlQuery coordinates(redirect);
        bool rowValid = false, columnValid = false;
        const auto row = coordinates.queryItemValue("row").toInt(&rowValid);
        const auto column = coordinates.queryItemValue("col").toInt(&columnValid);
        if (reply->error() != QNetworkReply::NoError || !rowValid || !columnValid || row < 0 || column < 0 || row > 2000 || column > 2000) {
            m_error = QStringLiteral("Nie udało się odnaleźć oryginalnego meteogramu ICM dla tej lokalizacji.");
        } else {
            auto source = m_endpoint.resolved(QUrl(QStringLiteral("/um/metco/mgram_pict.php")));
            QUrlQuery query;
            query.addQueryItem("ntype", "0u"); query.addQueryItem("row", QString::number(row)); query.addQueryItem("col", QString::number(column));
            query.addQueryItem("fdate", QUrlQuery(m_lookup).queryItemValue("fdate")); query.addQueryItem("lang", "pl");
            source.setQuery(query); m_source = source;
        }
        emit changed();
    });
}
