#pragma once
#include <QVariantMap>
#include <QVariantList>

// Stable identity and collection rules without settings, files or GUI objects.
class SavedPlaces {
public:
    SavedPlaces(QVariantList places={},QVariantList recent={}):m_places(std::move(places)),m_recent(std::move(recent)) {}
    QVariantList favorites() const;
    QVariantList recentPlaces() const;
    void savePlace(const QString &name, double latitude, double longitude);
    void removePlace(double latitude, double longitude);
    void rememberPlace(const QString &name, double latitude, double longitude);
    void toggleFavorite(const QString &name, double latitude, double longitude);
    bool isFavorite(double latitude, double longitude) const;
    QVariantMap placeAt(double latitude, double longitude) const;
    QVariantMap editPlace(const QString &id, const QString &name, double latitude, double longitude);
    QVariantMap deletePlace(const QString &id);
    QVariantMap restorePlace(const QVariantMap &place, int index);
private:
    QVariantList readPlaces(const QString &key) const { return key=="places/favorites"?m_places:m_recent; }
    void writePlaces(const QString &key,const QVariantList &places) { (key=="places/favorites"?m_places:m_recent)=places; }
    QVariantList m_places,m_recent;
};
