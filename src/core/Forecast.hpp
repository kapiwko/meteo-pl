#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QJsonDocument>
#include <QString>
#include <QVector>
#include <optional>

namespace Meteo {

struct Series {
    QString unit;
    qint64 firstTimestamp = 0;
    int interval = 0;
    QVector<std::optional<double>> values;

    [[nodiscard]] std::optional<double> at(qint64 timestamp) const;
};

struct Forecast {
    QDateTime modelRun;
    QMap<QString, Series> series;

    [[nodiscard]] qint64 firstTimestamp() const;
    [[nodiscard]] qint64 lastTimestamp() const;
};

[[nodiscard]] bool validLocation(double latitude, double longitude, const QString &model);
[[nodiscard]] bool knownModel(const QString &model);
[[nodiscard]] QString modelLabel(const QString &model);
[[nodiscard]] std::optional<Forecast> parseForecast(const QByteArray &json, QString *error = nullptr);
[[nodiscard]] std::optional<qint64> latestRun(const QByteArray &json, const QString &model);
[[nodiscard]] std::optional<QList<qint64>> availableRuns(const QJsonDocument &document, const QString &model);

} // namespace Meteo
