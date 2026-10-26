#include "ForecastRepository.hpp"
#include "core/Localization.hpp"
#include "network/DiskCacheBudget.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>

namespace Meteo
{
bool ForecastQuery::valid() const
{
    return validLocation(latitude, longitude, model) && requestedRun >= 0 && requestedRun <= 253402300799LL;
}
ForecastQuery ForecastQuery::canonical() const
{
    auto query = *this;
    query.latitude = QString::number(latitude, 'f', 6).toDouble();
    query.longitude = QString::number(longitude, 'f', 6).toDouble();
    if (query.latitude == 0)
        query.latitude = 0; // A unique representation for negative zero.
    if (query.longitude == 0)
        query.longitude = 0;
    return query;
}
QString ForecastQuery::pointKey() const
{
    const auto query = canonical();
    return model + ':' + QString::number(query.latitude, 'f', 6) + ':' + QString::number(query.longitude, 'f', 6);
}
ForecastRepository::ForecastRepository(QObject *parent, QUrl endpoint)
    : QObject(parent), m_endpoint(endpoint), m_client(nullptr, std::move(endpoint))
{
    connect(&m_runsRequest, &JsonRequest::failed, this, &ForecastRepository::catalogFailed);
    connect(&m_runsRequest, &JsonRequest::received, this, [this](const QJsonDocument &document) {
        if (const auto runs = availableRuns(document, m_catalogModel))
            emit runsReceived(*runs);
    });
    connect(&m_client, &MeteoClient::failed, this, &ForecastRepository::failed);
    connect(&m_client, &MeteoClient::runsReceived, this, &ForecastRepository::runsReceived);
    connect(&m_client, &MeteoClient::received, this, [this](const QByteArray &payload) {
        QString error;
        const auto forecast = parseForecast(payload, &error);
        if (!forecast) {
            emit failed(error);
            return;
        }
        ForecastSnapshot snapshot{m_query, *forecast, QDateTime::currentDateTimeUtc(), false,
                                  QCryptographicHash::hash(payload, QCryptographicHash::Sha256)};
        save(snapshot, payload);
        emit received(snapshot);
    });
}
void ForecastRepository::cancel()
{
    m_client.cancel();
    m_runsRequest.cancel();
}
void ForecastRepository::loadRuns(const QString &model)
{
    if (!knownModel(model))
        return;
    m_catalogModel = model;
    m_runsRequest.get(m_endpoint.resolved(QUrl(QStringLiteral("/meteorograms/available"))), 300, false,
                      [model](const QJsonDocument &document) { return availableRuns(document, model).has_value(); });
}
void ForecastRepository::fetch(const ForecastQuery &query)
{
    cancel();
    if (!query.valid()) {
        emit failed(
            Meteo::i18n("This location is outside the model area. Choose GFS for locations outside Central Europe."));
        return;
    }
    m_query = query.canonical();
    m_client.fetch(m_query.latitude, m_query.longitude, m_query.model, m_query.requestedRun);
}
QString ForecastRepository::cachePath(const ForecastQuery &query)
{
    const auto hash = QCryptographicHash::hash(query.pointKey().toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/meteo-pl/") +
           QString::fromLatin1(hash) +
           (query.requestedRun > 0 ? QStringLiteral("-%1").arg(query.requestedRun) : QString()) +
           QStringLiteral(".json");
}
std::optional<ForecastSnapshot> ForecastRepository::cached(const ForecastQuery &query) const
{
    if (!query.valid())
        return std::nullopt;
    QFile file(cachePath(query));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 3 * 1024 * 1024)
        return std::nullopt;
    const auto envelope = QJsonDocument::fromJson(file.readAll()).object();
    const ForecastQuery stored{envelope.value("latitude").toDouble(), envelope.value("longitude").toDouble(),
                               envelope.value("model").toString(), envelope.value("requestedRun").toInteger()};
    if (!stored.valid() || stored.pointKey() != query.pointKey() || stored.requestedRun != query.requestedRun)
        return std::nullopt;
    const auto saved = QDateTime::fromString(envelope.value("savedAt").toString(), Qt::ISODate);
    if (!saved.isValid() || saved > QDateTime::currentDateTimeUtc())
        return std::nullopt;
    const auto payload = envelope.value("payload").toString().toUtf8();
    const auto forecast = parseForecast(payload);
    if (!forecast)
        return std::nullopt;
    touchCacheEntry(file.fileName());
    return ForecastSnapshot{query.canonical(), *forecast, saved, true,
                            QCryptographicHash::hash(payload, QCryptographicHash::Sha256)};
}
void ForecastRepository::save(const ForecastSnapshot &snapshot, const QByteArray &payload) const
{
    const auto &query = snapshot.query;
    const auto path = cachePath(query);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    const QJsonObject envelope{{"model", query.model},
                               {"latitude", query.latitude},
                               {"longitude", query.longitude},
                               {"requestedRun", query.requestedRun},
                               {"savedAt", snapshot.fetchedAt.toString(Qt::ISODate)},
                               {"payload", QString::fromUtf8(payload)}};
    const auto bytes = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    if (file.write(bytes) == bytes.size() && file.commit())
        pruneDiskCache(QFileInfo(path).absolutePath(), {QStringLiteral("*.json")}, forecastCacheBudget, path);
}
} // namespace Meteo
