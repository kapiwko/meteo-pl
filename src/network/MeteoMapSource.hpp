#pragma once
#include <QDateTime>
#include <QJsonDocument>
#include <QUrl>
#include <QVector>
#include <optional>

namespace Meteo {
struct MapRun {
    QDateTime date;
    int firstStep = 1, lastStep = 1, stepHours = 1;
    QString sourceRevision;
    [[nodiscard]] int boundedStep(int step) const;
};

// Supplier protocol only: no selection state, locale, QML engine or requests.
class MeteoMapSource {
  public:
    explicit MeteoMapSource(QUrl endpoint = QUrl(QStringLiteral("https://mapy.meteo.pl")));
    [[nodiscard]] QUrl runsUrl(const QString &model) const;
    [[nodiscard]] QUrl tileUrl(const QString &layer, const MapRun &run, int step, int x, int y, int zoom) const;
    [[nodiscard]] QUrl legendUrl(const QString &layer) const;
    [[nodiscard]] static std::optional<QVector<MapRun>> parseRuns(const QJsonDocument &document, const QString &model);

  private:
    QUrl m_endpoint;
};
} // namespace Meteo
