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
    if (weather)
        m_weatherConnection = connect(weather, &WeatherModel::locationChanged, this, [this] { schedule(); });
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
    emit visiblePlacesChanged();
    schedule();
}
void SavedPlaceController::componentComplete()
{
    m_ready = true;
    if (m_visibleKeys.contains("*")) rememberCurrent();
    else reconcile();
}
QString SavedPlaceController::placeKey(double latitude, double longitude)
{
    return QString::number(latitude, 'f', 6) + "," + QString::number(longitude, 'f', 6);
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
    const auto key = [](const QVariant &value) {
        const auto place = value.toMap();
        return placeKey(place["latitude"].toDouble(), place["longitude"].toDouble());
    };
    if (!m_visibleKeys.contains("*"))
        result.removeIf([&](const QVariant &value) { return !m_visibleKeys.contains(key(value)); });
    const auto rank = [&](const QVariant &value) {
        const auto index = m_orderKeys.indexOf(key(value));
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
void SavedPlaceController::select(const QVariantMap &place)
{
    emit locationSelected(place.value("name").toString(), place.value("latitude").toDouble(),
                          place.value("longitude").toDouble());
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
    for (const auto &entry : available) {
        const auto place = entry.toMap();
        if (!m_weather->locationName().isEmpty() && Meteo::samePlace(place["latitude"].toDouble(), place["longitude"].toDouble(),
                m_weather->latitude(), m_weather->longitude())) {
            if (place["name"].toString() != m_weather->locationName()) select(place);
            return;
        }
    }
    if (!available.isEmpty())
        select(available.first().toMap());
    else if (!m_weather->locationName().isEmpty())
        emit locationSelected({}, 0, 0);
}
