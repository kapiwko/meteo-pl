#include "SavedPlaceController.hpp"
#include "core/Locations.hpp"
#include <algorithm>
#include <utility>

SavedPlaceController::SavedPlaceController(QObject *parent) : QObject(parent)
{
    m_synchronize.setSingleShot(true);
    m_synchronize.setInterval(0);
    connect(&m_synchronize, &QTimer::timeout, this, [this] {
        if (std::exchange(m_collectionChanged, false))
            reconcile();
        else
            rememberCurrent();
    });
}
void SavedPlaceController::setWeather(WeatherModel *weather)
{
    if (weather == m_weather)
        return;
    disconnect(m_weatherConnection);
    m_weather = weather;
    m_selectedId.clear();
    if (weather)
        m_weatherConnection = connect(weather, &WeatherModel::locationChanged, this, [this] { trackSelection(); schedule(); });
    trackSelection();
    emit weatherChanged();
    schedule();
}
void SavedPlaceController::setPreferences(UserPreferences *preferences)
{
    if (preferences == m_preferences)
        return;
    disconnect(m_preferencesConnection);
    m_preferences = preferences;
    if (preferences)
        m_preferencesConnection =
            connect(preferences, &UserPreferences::placesChanged, this, [this] { emit visiblePlacesChanged(); schedule(true); });
    emit preferencesChanged();
    trackSelection();
    emit visiblePlacesChanged();
    schedule();
}
void SavedPlaceController::componentComplete()
{
    m_ready = true;
    if (m_visibleKeys.contains("*")) rememberCurrent();
    else reconcile();
}
QString SavedPlaceController::placeKey(double latitude, double longitude) const
{
    const auto place = m_preferences ? m_preferences->placeAt(latitude, longitude) : QVariantMap{};
    if (!place.value("id").toString().isEmpty()) return place["id"].toString();
    return QString::number(latitude, 'f', 6) + "," + QString::number(longitude, 'f', 6);
}
bool SavedPlaceController::includesPlace(const QStringList &keys, double latitude, double longitude) const
{
    const auto id = placeKey(latitude, longitude);
    if (keys.contains("*") || keys.contains(id)) return true;
    const auto legacy = QString::number(latitude, 'f', 6) + ',' + QString::number(longitude, 'f', 6);
    if (!keys.contains(legacy)) return false;
    // A migrated ID may name a place which has since moved away. That key
    // must not also select a newly added point at its original coordinates.
    if (m_preferences)
        for (const auto &entry : m_preferences->favorites())
            if (entry.toMap().value("id").toString() == legacy) return false;
    return true;
}
void SavedPlaceController::setVisibleKeys(const QStringList &keys)
{
    if (m_visibleKeys == keys) return;
    m_visibleKeys = keys;
    emit visiblePlacesChanged();
    schedule(true);
}
void SavedPlaceController::setOrderKeys(const QStringList &keys)
{
    if (m_orderKeys == keys) return;
    m_orderKeys = keys;
    emit visiblePlacesChanged();
    schedule(true);
}
QVariantList SavedPlaceController::places() const
{
    auto result = m_preferences ? m_preferences->favorites() : QVariantList{};
    const auto key = [this](const QVariant &value) {
        const auto place = value.toMap();
        return placeKey(place["latitude"].toDouble(), place["longitude"].toDouble());
    };
    if (!m_visibleKeys.contains("*"))
        result.removeIf([&](const QVariant &value) {
            const auto place = value.toMap();
            return !includesPlace(m_visibleKeys, place["latitude"].toDouble(), place["longitude"].toDouble());
        });
    const auto rank = [&](const QVariant &value) {
        auto index = m_orderKeys.indexOf(key(value));
        if (index < 0) {
            const auto place = value.toMap();
            const auto lat = place["latitude"].toDouble(), lon = place["longitude"].toDouble();
            const auto legacy = QString::number(lat, 'f', 6) + ',' + QString::number(lon, 'f', 6);
            if (includesPlace({legacy}, lat, lon)) index = m_orderKeys.indexOf(legacy);
        }
        return index < 0 ? m_orderKeys.size() : index;
    };
    std::stable_sort(result.begin(), result.end(), [&](const QVariant &a, const QVariant &b) { return rank(a) < rank(b); });
    return result;
}
void SavedPlaceController::schedule(bool collectionChanged)
{
    if (!m_ready)
        return;
    m_collectionChanged |= collectionChanged;
    m_synchronize.start();
}
void SavedPlaceController::trackSelection()
{
    m_selectedId = m_weather && m_preferences && !m_weather->locationName().isEmpty()
        ? m_preferences->placeAt(m_weather->latitude(), m_weather->longitude()).value("id").toString() : QString{};
}
void SavedPlaceController::select(const QVariantMap &place)
{
    emit locationSelected(place.value("name").toString(), place.value("latitude").toDouble(),
                          place.value("longitude").toDouble());
    m_selectedId = place.value("id").toString();
}
void SavedPlaceController::rememberCurrent()
{
    if (!m_weather || !m_preferences)
        return;
    if (m_weather->locationName().isEmpty()) {
        reconcile();
        return;
    }
    const auto place = m_preferences->placeAt(m_weather->latitude(), m_weather->longitude());
    m_selectedId = place.value("id").toString();
    const auto key = placeKey(place.isEmpty() ? m_weather->latitude() : place["latitude"].toDouble(),
                              place.isEmpty() ? m_weather->longitude() : place["longitude"].toDouble());
    if (!m_visibleKeys.contains("*") && !m_visibleKeys.contains(key)) emit includePlaceRequested(key);
    if (place.isEmpty())
        m_preferences->savePlace(m_weather->locationName(), m_weather->latitude(), m_weather->longitude());
    else if (place["name"].toString() != m_weather->locationName())
        select(place);
}
void SavedPlaceController::reconcile()
{
    if (!m_weather || !m_preferences)
        return;
    const auto available = places();
    if (!m_selectedId.isEmpty())
        for (const auto &entry : available) {
            const auto place = entry.toMap();
            if (place.value("id").toString() != m_selectedId) continue;
            if (place["name"].toString() != m_weather->locationName()
                || !Meteo::samePlace(place["latitude"].toDouble(), place["longitude"].toDouble(), m_weather->latitude(), m_weather->longitude()))
                select(place);
            return;
        }
    for (const auto &entry : available) {
        const auto place = entry.toMap();
        if (!m_weather->locationName().isEmpty() && Meteo::samePlace(place["latitude"].toDouble(), place["longitude"].toDouble(),
                m_weather->latitude(), m_weather->longitude())) {
            m_selectedId = place.value("id").toString();
            if (place["name"].toString() != m_weather->locationName()) select(place);
            return;
        }
    }
    if (!available.isEmpty())
        select(available.first().toMap());
    else if (!m_weather->locationName().isEmpty()) {
        m_selectedId.clear();
        emit locationSelected({}, 0, 0);
    }
}
