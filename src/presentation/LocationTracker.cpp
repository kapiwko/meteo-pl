#include "LocationTracker.hpp"
#include "core/Localization.hpp"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocationPermission>
#include <utility>

LocationTracker::LocationTracker(QObject *parent, QGeoPositionInfoSource *source, int lookupTimeoutMs, PermissionRequest permissionRequest)
    : QObject(parent), m_source(source), m_permissionRequest(std::move(permissionRequest)), m_injectedSource(source != nullptr)
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
    if (foreground) { if (m_enabled && !(m_oneShot && hasPosition())) start(); }
    else {
        m_lookupDeadline.stop();
        if (m_source) m_source->stopUpdates();
        m_busy = false;
        emit stateChanged();
    }
}
void LocationTracker::retry() { if (m_enabled && !(m_oneShot && hasPosition())) start(); }
void LocationTracker::fail(const QString &error)
{
    m_lookupDeadline.stop();
    m_busy = false;
    if (m_source) m_source->stopUpdates();
    m_error = error;
    emit stateChanged();
}
void LocationTracker::start()
{
    if (!m_enabled || !m_foreground) return;
    m_error.clear();
    m_busy = true;
    emit stateChanged();
    // Returning from the background resumes the activity indicator even when
    // the existing system permission prompt has not delivered its result yet.
    if (m_permissionPending) return;
    if (m_injectedSource && !m_permissionRequest) { startSource(); return; }
    m_permissionPending = true;
    const auto completed = [this](Qt::PermissionStatus status) {
        m_permissionPending = false;
        if (!m_enabled || !m_foreground) return;
        if (status == Qt::PermissionStatus::Granted) startSource();
        else fail(Meteo::i18n("Location access was denied. Allow location access for Meteo in the system settings, then try again."));
    };
    if (m_permissionRequest) { m_permissionRequest(this, completed); return; }
    QLocationPermission permission;
    permission.setAccuracy(QLocationPermission::Approximate);
    permission.setAvailability(QLocationPermission::WhenInUse);
    const auto status = qApp->checkPermission(permission);
    if (status == Qt::PermissionStatus::Undetermined)
        qApp->requestPermission(permission, this, [completed](const QPermission &result) { completed(result.status()); });
    else completed(status);
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
    if (!m_enabled || !m_foreground || (m_oneShot && hasPosition()) || !position.isValid()) return;
    const auto age = position.timestamp().secsTo(QDateTime::currentDateTimeUtc());
    if (age > 300 || age < -60) return;
    m_lookupDeadline.stop();
    m_busy = false;
    m_error.clear();
    const auto coordinate = position.coordinate();
    if (m_oneShot && m_source) m_source->stopUpdates();
    // A weather grid spans kilometres. Avoid new downloads for GPS jitter.
    if (!m_coordinate.isValid() || m_coordinate.distanceTo(coordinate) >= 1000) {
        m_coordinate = coordinate;
        m_placeName.clear();
        emit positionAccepted(coordinate.latitude(), coordinate.longitude());
        if (!m_injectedSource && !m_oneShot) m_geocoder.lookup(coordinate.latitude(), coordinate.longitude());
    }
    emit stateChanged();
}
