#include "SavedPlaceController.hpp"
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
            connect(preferences, &UserPreferences::placesChanged, this, [this] { schedule(true); });
    emit preferencesChanged();
    schedule();
}
void SavedPlaceController::componentComplete()
{
    m_ready = true;
    rememberCurrent();
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
    if (place.isEmpty())
        m_preferences->savePlace(m_weather->locationName(), m_weather->latitude(), m_weather->longitude());
    else if (place["name"].toString() != m_weather->locationName())
        select(place);
}
void SavedPlaceController::reconcile()
{
    if (!m_weather || !m_preferences)
        return;
    const auto place = m_preferences->placeAt(m_weather->latitude(), m_weather->longitude());
    if (!place.isEmpty() && !m_weather->locationName().isEmpty()) {
        if (place["name"].toString() != m_weather->locationName())
            select(place);
        return;
    }
    const auto places = m_preferences->favorites();
    if (!places.isEmpty())
        select(places.first().toMap());
    else if (!m_weather->locationName().isEmpty())
        emit locationSelected({}, 0, 0);
}
