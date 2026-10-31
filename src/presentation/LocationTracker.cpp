#include "LocationTracker.hpp"
#include "core/Localization.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocationPermission>

LocationTracker::LocationTracker(QObject *parent, QGeoPositionInfoSource *source, int lookupTimeoutMs)
    : QObject(parent), m_source(source), m_injectedSource(source != nullptr)
{
    m_lookupDeadline.setSingleShot(true);
    m_lookupDeadline.setInterval(qMax(1, lookupTimeoutMs));
    connect(&m_lookupDeadline, &QTimer::timeout, this, [this] {
        if (m_enabled && m_foreground && m_busy)
            fail(Meteo::i18n("Location lookup timed out. You can choose a point on the map."));
    });
    if (source) source->setParent(this);
    if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        m_foreground = app->applicationState() == Qt::ApplicationActive;
        connect(app, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) { setForeground(state == Qt::ApplicationActive); });
    }
    connect(&m_geocoder, &ReverseGeocoder::resolved, this, [this](const QString &name) {
        if (!m_enabled) return;
        m_placeName = name;
        emit stateChanged();
    });
    connect(&m_geocoder, &ReverseGeocoder::stateChanged, this, &LocationTracker::stateChanged);
}
void LocationTracker::setEnabled(bool enabled)
{
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    m_lookupDeadline.stop();
    m_error.clear();
    m_busy = false;
    m_coordinate = {};
    m_placeName.clear();
    m_geocoder.cancel();
    if (m_source) m_source->stopUpdates();
    emit stateChanged();
    if (enabled) start();
}
void LocationTracker::setForeground(bool foreground)
{
    if (m_foreground == foreground) return;
    m_foreground = foreground;
    if (foreground) { if (m_enabled) start(); }
    else {
        m_lookupDeadline.stop();
        if (m_source) m_source->stopUpdates();
        m_busy = false;
        emit stateChanged();
    }
}
void LocationTracker::retry() { if (m_enabled) start(); }
void LocationTracker::fail(const QString &error)
{
    m_lookupDeadline.stop();
    m_busy = false;
    m_error = error;
    emit stateChanged();
}
void LocationTracker::start()
{
    if (!m_enabled || !m_foreground || m_permissionPending) return;
    m_error.clear();
    m_busy = true;
    emit stateChanged();
    if (m_injectedSource) { startSource(); return; }
    QLocationPermission permission;
    permission.setAccuracy(QLocationPermission::Approximate);
    permission.setAvailability(QLocationPermission::WhenInUse);
    switch (qApp->checkPermission(permission)) {
    case Qt::PermissionStatus::Granted: startSource(); break;
    case Qt::PermissionStatus::Denied:
        fail(Meteo::i18n("Location access was denied. Allow location access for Meteo in the system settings, then try again."));
        break;
    case Qt::PermissionStatus::Undetermined:
        m_permissionPending = true;
        qApp->requestPermission(permission, this, [this](const QPermission &result) {
            m_permissionPending = false;
            if (!m_enabled || !m_foreground) return;
            if (result.status() == Qt::PermissionStatus::Granted) startSource();
            else fail(Meteo::i18n("Location access was denied. Allow location access for Meteo in the system settings, then try again."));
        });
        break;
    }
}
void LocationTracker::startSource()
{
    if (!m_source) m_source = QGeoPositionInfoSource::createDefaultSource(this);
    if (!m_source) {
        fail(Meteo::i18n("The location service is unavailable. Choose a place or a point on the map."));
        return;
    }
    if (!m_sourceConnected) {
      m_sourceConnected = true;
      connect(m_source, &QGeoPositionInfoSource::positionUpdated, this, &LocationTracker::acceptPosition);
      connect(m_source, &QGeoPositionInfoSource::errorOccurred, this, [this](QGeoPositionInfoSource::Error error) {
        if (!m_enabled || !m_foreground || error == QGeoPositionInfoSource::NoError) return;
        fail(error == QGeoPositionInfoSource::UpdateTimeoutError
            ? Meteo::i18n("Location lookup timed out. You can choose a point on the map.")
            : Meteo::i18n("Could not determine your location. Check the location service or choose a point on the map."));
      });
    }
    m_source->setUpdateInterval(60000);
    m_lookupDeadline.start();
    m_source->startUpdates();
    m_source->requestUpdate(m_lookupDeadline.interval());
}
void LocationTracker::acceptPosition(const QGeoPositionInfo &position)
{
    if (!m_enabled || !m_foreground || !position.isValid()) return;
    const auto age = position.timestamp().secsTo(QDateTime::currentDateTimeUtc());
    if (age > 300 || age < -60) return;
    m_lookupDeadline.stop();
    m_busy = false;
    m_error.clear();
    const auto coordinate = position.coordinate();
    // A weather grid spans kilometres. Avoid new downloads for GPS jitter.
    if (!m_coordinate.isValid() || m_coordinate.distanceTo(coordinate) >= 1000) {
        m_coordinate = coordinate;
        m_placeName.clear();
        emit positionAccepted(coordinate.latitude(), coordinate.longitude());
        if (!m_injectedSource) m_geocoder.lookup(coordinate.latitude(), coordinate.longitude());
    }
    emit stateChanged();
}
