#include "Locations.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <cmath>

namespace Meteo {
QVariantList parseLocations(const QJsonDocument &document)
{
    QVariantList results;
    QSet<QString> seen;
    const bool photon = document.isObject();
    const auto entries = photon ? document.object().value("features").toArray() : document.array();
    for (const auto &entry : entries) {
        const auto object = entry.toObject();
        const auto properties = photon ? object.value("properties").toObject() : object.value("tags").toObject();
        const auto coordinates = object.value("geometry").toObject().value("coordinates").toArray();
        QString name = photon ? properties.value("name").toString() : object.value("city").toString();
        if (name.isEmpty()) name = properties.value("town").toString();
        const auto latitude = photon ? coordinates.at(1) : object.value("lat");
        const auto longitude = photon ? coordinates.at(0) : object.value("lon");
        const auto number = [](const QJsonValue &value) {
            if (!value.isString()) return value.toDouble(qQNaN());
            bool ok = false;
            const auto result = value.toString().toDouble(&ok);
            return ok ? result : qQNaN();
        };
        const double lat = number(latitude);
        const double lon = number(longitude);
        if (name.isEmpty() || !std::isfinite(lat) || !std::isfinite(lon) || std::abs(lat) > 90 || std::abs(lon) > 180)
            continue;
        const auto key = name + QString::number(lat, 'f', 5) + ':' + QString::number(lon, 'f', 5);
        if (seen.contains(key)) continue;
        seen.insert(key);
        QStringList context;
        for (const auto &field : {"county", "state", "country"}) {
            const auto value = properties.value(field).toString();
            if (!value.isEmpty() && value != name && !context.contains(value)) context.append(value);
        }
        results.append(QVariantMap{{"name", name}, {"latitude", lat}, {"longitude", lon},
                                   {"description", context.join(QStringLiteral(" · "))}});
        if (results.size() >= 30) break;
    }
    return results;
}
}
