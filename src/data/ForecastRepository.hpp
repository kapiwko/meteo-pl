#pragma once
#include "core/Forecast.hpp"
#include "network/JsonRequest.hpp"
#include "network/MeteoClient.hpp"
#include <QObject>

namespace Meteo
{
struct ForecastQuery {
    double latitude = 0, longitude = 0;
    QString model;
    qint64 requestedRun = 0;
    bool valid() const;
    ForecastQuery canonical() const;
    QString pointKey() const;
};
struct ForecastSnapshot {
    ForecastQuery query;
    Forecast forecast;
    QDateTime fetchedAt;
    bool usingCache = false;
    QByteArray sourceRevision;
};

class ForecastRepository : public QObject
{
    Q_OBJECT
public:
    explicit ForecastRepository(QObject *parent = nullptr,
                                QUrl endpoint = QUrl(QStringLiteral("https://devmgramapi.meteo.pl")));
    void fetch(const ForecastQuery &query);
    void cancel();
    void loadRuns(const QString &model);
    std::optional<ForecastSnapshot> cached(const ForecastQuery &query) const;
signals:
    void received(const Meteo::ForecastSnapshot &snapshot);
    void failed(const QString &error);
    void catalogFailed(const QString &error);
    void runsReceived(const QList<qint64> &runs);

private:
    static QString cachePath(const ForecastQuery &query);
    void save(const ForecastSnapshot &snapshot, const QByteArray &payload) const;
    QUrl m_endpoint;
    MeteoClient m_client;
    JsonRequest m_runsRequest;
    QString m_catalogModel;
    ForecastQuery m_query;
};
} // namespace Meteo
Q_DECLARE_METATYPE(Meteo::ForecastSnapshot)
