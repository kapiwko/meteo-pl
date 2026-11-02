#include "SavedPlaces.hpp"
#include "core/Localization.hpp"
#include "core/Locations.hpp"
#include <QUuid>
#include <cmath>
#include <algorithm>

QVariantList SavedPlaces::favorites() const
{
    auto places = readPlaces(QStringLiteral("places/favorites"));
    for (auto &entry : places) {
        auto place = entry.toMap();
        if (place.value("id").toString().isEmpty()) {
            place["id"] = QString::number(place["latitude"].toDouble(), 'f', 6) + ','
                + QString::number(place["longitude"].toDouble(), 'f', 6);
            entry = place;
        }
    }
    return places;
}

QVariantList SavedPlaces::recentPlaces() const { return readPlaces(QStringLiteral("places/recent")); }

bool SavedPlaces::isFavorite(double latitude, double longitude) const
{
    return !placeAt(latitude, longitude).isEmpty();
}

QVariantMap SavedPlaces::placeAt(double latitude, double longitude) const
{
    for (const auto &value : favorites()) {
        const auto place = value.toMap();
        if (Meteo::samePlace(place["latitude"].toDouble(), place["longitude"].toDouble(), latitude, longitude))
            return place;
    }
    return {};
}

void SavedPlaces::savePlace(const QString &name, double latitude, double longitude)
{
    // Explicit saves are upserts. For the same coordinates, the last committed
    // save/remove wins; changes to other places are preserved under the lock.
    if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) ||
        std::abs(latitude) > 90 || std::abs(longitude) > 180)
        return;
    auto places = favorites();
    for (auto &entry : places) {
        auto place = entry.toMap();
        if (!Meteo::samePlace(place["latitude"].toDouble(), place["longitude"].toDouble(), latitude, longitude))
            continue;
        if (place["name"].toString() == name.trimmed())
            return;
        place["name"] = name.trimmed();
        entry = place;
        writePlaces("places/favorites", places);
        return;
    }
    places.append(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"name", name.trimmed()}, {"latitude", latitude}, {"longitude", longitude}});
    writePlaces("places/favorites", places);
}

QVariantMap SavedPlaces::editPlace(const QString &id, const QString &name, double latitude, double longitude)
{
    QVariantMap result{{"ok", false}};
    if (name.trimmed().isEmpty() || name.trimmed().size() > 100 || !std::isfinite(latitude)
        || !std::isfinite(longitude) || std::abs(latitude) > 90 || std::abs(longitude) > 180) {
        result["error"] = Meteo::i18n("Enter a name and valid coordinates.");
        return result;
    }
    auto places = favorites();
    qsizetype index = -1;
    for (qsizetype i = 0; i < places.size(); ++i) {
        const auto place = places[i].toMap();
        if (!id.isEmpty() && place["id"].toString() == id) index = i;
        else if (Meteo::samePlace(latitude, longitude, place["latitude"].toDouble(), place["longitude"].toDouble())) {
            result["error"] = Meteo::i18n("This place is already in your list.");
            return result;
        }
    }
    if (!id.isEmpty() && index < 0) {
        result["error"] = Meteo::i18n("This place no longer exists. Close the editor and choose another place.");
        return result;
    }
    const QVariantMap place{{"id", id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : id},
        {"name", name.trimmed()}, {"latitude", latitude}, {"longitude", longitude}};
    if (index < 0) places.append(place);
    else places[index] = place;
    writePlaces("places/favorites", places);
    result = {{"ok", true}, {"place", place}};
    return result;
}

QVariantMap SavedPlaces::deletePlace(const QString &id)
{
    QVariantMap result{{"ok", false}, {"error", Meteo::i18n("This place no longer exists.")}};
    auto places = favorites();
    for (qsizetype i = 0; i < places.size(); ++i) {
        const auto place = places[i].toMap();
        if (place["id"].toString() != id) continue;
        places.removeAt(i);
        writePlaces("places/favorites", places);
        result = {{"ok", true}, {"place", place}, {"index", int(i)}};
        return result;
    }
    return result;
}

QVariantMap SavedPlaces::restorePlace(const QVariantMap &place, int index)
{
    QVariantMap result{{"ok", false}};
    const auto id = place.value("id").toString();
    const auto name = place.value("name").toString();
    const auto lat = place.value("latitude", qQNaN()).toDouble(), lon = place.value("longitude", qQNaN()).toDouble();
    if (id.isEmpty() || name.trimmed().isEmpty() || !std::isfinite(lat) || !std::isfinite(lon)
        || std::abs(lat) > 90 || std::abs(lon) > 180) {
        result["error"] = Meteo::i18n("This place cannot be restored.");
        return result;
    }
    auto places = favorites();
    for (const auto &entry : places) {
        const auto existing = entry.toMap();
        if (existing["id"].toString() == id || Meteo::samePlace(lat, lon, existing["latitude"].toDouble(), existing["longitude"].toDouble())) {
            result["error"] = Meteo::i18n("This place is already in your list.");
            return result;
        }
    }
    places.insert(std::clamp(index, 0, int(places.size())), place);
    writePlaces("places/favorites", places);
    result = {{"ok", true}, {"place", place}};
    return result;
}

void SavedPlaces::removePlace(double latitude, double longitude)
{
    auto places = favorites();
    const auto count = places.size();
    places.removeIf([&](const QVariant &entry) {
        const auto place = entry.toMap();
        return Meteo::samePlace(place["latitude"].toDouble(), place["longitude"].toDouble(), latitude, longitude);
    });
    if (places.size() != count)
        writePlaces("places/favorites", places);
}

void SavedPlaces::rememberPlace(const QString &name, double latitude, double longitude)
{
    if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) ||
        std::abs(latitude) > 90 || std::abs(longitude) > 180)
        return;
    auto saved = placeAt(latitude, longitude);
    if (saved.isEmpty()) {
        savePlace(name, latitude, longitude);
        saved = placeAt(latitude, longitude);
    }
    auto places = recentPlaces();
    places.removeIf([latitude, longitude](const QVariant &v) {
        const auto p = v.toMap();
        return Meteo::samePlace(p["latitude"].toDouble(), p["longitude"].toDouble(), latitude, longitude);
    });
    places.prepend(saved);
    while (places.size() > 12)
        places.removeLast();
    writePlaces("places/recent", places);
}

void SavedPlaces::toggleFavorite(const QString &name, double latitude, double longitude)
{
    if (name.trimmed().isEmpty() || !std::isfinite(latitude) || !std::isfinite(longitude) ||
        std::abs(latitude) > 90 || std::abs(longitude) > 180)
        return;
    auto places = favorites();
    const bool removed = places.removeIf([latitude, longitude](const QVariant &v) {
        const auto p = v.toMap();
        return Meteo::samePlace(p["latitude"].toDouble(), p["longitude"].toDouble(), latitude, longitude);
    }) > 0;
    if (!removed)
        places.append(QVariantMap{{"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"name", name.trimmed()}, {"latitude", latitude}, {"longitude", longitude}});
    writePlaces("places/favorites", places);
}
