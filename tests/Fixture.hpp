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
                       {"airtmp_min", field("Celsius", {8.0, 9.0, 8.0, 9.0})},
                       {"airtmp_max", field("Celsius", {12.0, 14.0, 12.0, 13.0})},
                       {"wchill_point", field("Celsius", {8.0, 10.0, 8.0, 10.0})},
                       {"grdtmp_min", field("Celsius", {5.0, 6.0, 7.0, 4.0})},
                       {"grdtmp_max", field("Celsius", {13.0, 15.0, 14.0, 12.0})},
                       {"dwptmp_point", field("Celsius", {5.0, 6.0, 7.0, 5.0})},
                       {"pcpttl_aver", field("kg/m2", {0.0, 0.3, 1.2, 0.0})},
                       {"pcpttl_max", field("kg/m2", {0.0, 0.7, 2.0, 0.0})},
                       {"pcpttlprob_point", field("%", {0, 20, 80, 0})},
                       {"pcpttl_type_max", field("1", {0, 1, 2, 3})},
                       {"slpres_point", field("Pa", {101200, 101300, 101250, 101100})},
                       {"wind10_sd_true_prev_point", field("m/s", {2.0, 3.0, 4.0, 1.0})},
                       {"wind_gust_max", field("m/s", {5.0, 6.0, 8.0, 3.0})},
                       {"wind10_dr_deg_true_prev_point", field("degrees", {0, 90, 180, 270})},
                       {"realhum_aver", field("%", {60, 70, 80, 60})},
                       {"fog_max", field("1", {0.0, 0.1, 0.2, 0.0})},
                       {"visibl_min", field("m", {20000, 10000, 5000, 15000})},
                       {"cldbse25", field("m", {1000, 800, 600, 900})},
                       {"cldtop", field("m", {6000, 5000, 4000, 5000})},
                       {"storm_max", field("1", {0, 1, 2, 0})},
                       {"flash_max", field("1/km2/h", {0, 0.2, 0.5, 0})},
                       {"cldtot_aver", field("1", {0.1, 0.4, 0.8, 0.2})}};
    return QJsonDocument(QJsonObject{{"data", fields}, {"fstart", run}}).toJson(QJsonDocument::Compact);
}
