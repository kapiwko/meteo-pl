#pragma once
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

inline QByteArray forecastFixture(qint64 first = QDateTime::currentSecsSinceEpoch() - 1800)
{
    const auto run = QDateTime::fromSecsSinceEpoch(first, QTimeZone::UTC).toString(Qt::ISODate);
    auto field = [first, run](const QString &unit, const QJsonArray &values) {
        return QJsonObject{{"data", values}, {"unit", unit}, {"first_timestamp", QString::number(first)},
                           {"interval", 3600}, {"fstart", run}};
    };
    // Synthetic data only; no redistribution of ICM forecast data.
    QJsonObject fields{{"airtmp_point", field("Celsius", {10.0, 12.0, 9.0, 11.0})},
                       {"dwptmp_point", field("Celsius", {5.0, 6.0, 7.0, 5.0})},
                       {"pcpttl_aver", field("kg/m2", {0.0, 0.3, 1.2, 0.0})},
                       {"slpres_point", field("Pa", {101200, 101300, 101250, 101100})},
                       {"wind10_sd_true_prev_point", field("m/s", {2.0, 3.0, 4.0, 1.0})},
                       {"wind_gust_max", field("m/s", {5.0, 6.0, 8.0, 3.0})},
                       {"cldtot_aver", field("1", {0.1, 0.4, 0.8, 0.2})}};
    return QJsonDocument(QJsonObject{{"data", fields}, {"fstart", run}}).toJson(QJsonDocument::Compact);
}
