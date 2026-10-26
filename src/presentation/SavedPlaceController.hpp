#pragma once
#include "MeteoUiExport.hpp"
#include "UserPreferences.hpp"
#include "WeatherModel.hpp"
#include <QPointer>
#include <QQmlParserStatus>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// One controller per view; the collection is shared, the selected place is not.
class METEO_UI_EXPORT SavedPlaceController : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(WeatherModel *weather READ weather WRITE setWeather NOTIFY weatherChanged REQUIRED)
    Q_PROPERTY(UserPreferences *preferences READ preferences WRITE setPreferences NOTIFY preferencesChanged REQUIRED)
    Q_PROPERTY(QStringList visibleKeys READ visibleKeys WRITE setVisibleKeys NOTIFY visiblePlacesChanged)
    Q_PROPERTY(QStringList orderKeys READ orderKeys WRITE setOrderKeys NOTIFY visiblePlacesChanged)
    Q_PROPERTY(QVariantList places READ places NOTIFY visiblePlacesChanged)
public:
    explicit SavedPlaceController(QObject *parent = nullptr);
    WeatherModel *weather() const { return m_weather; }
    UserPreferences *preferences() const { return m_preferences; }
    void setWeather(WeatherModel *weather);
    void setPreferences(UserPreferences *preferences);
    QStringList visibleKeys() const { return m_visibleKeys; }
    QStringList orderKeys() const { return m_orderKeys; }
    void setVisibleKeys(const QStringList &keys);
    void setOrderKeys(const QStringList &keys);
    QVariantList places() const;
    Q_INVOKABLE static QString placeKey(double latitude, double longitude);
    void classBegin() override {}
    void componentComplete() override;
signals:
    void weatherChanged();
    void preferencesChanged();
    void visiblePlacesChanged();
    void includePlaceRequested(const QString &key);
    void locationSelected(const QString &name, double latitude, double longitude);

private:
    void schedule(bool collectionChanged = false);
    void rememberCurrent();
    void reconcile();
    void select(const QVariantMap &place);
    QPointer<WeatherModel> m_weather;
    QPointer<UserPreferences> m_preferences;
    QMetaObject::Connection m_weatherConnection, m_preferencesConnection;
    QTimer m_synchronize;
    bool m_ready = false, m_collectionChanged = false;
    QStringList m_visibleKeys{QStringLiteral("*")}, m_orderKeys;
};
