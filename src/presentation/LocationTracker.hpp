#pragma once
#include "MeteoUiExport.hpp"
#include "ReverseGeocoder.hpp"
#include <QGeoCoordinate>
#include <QGeoPositionInfoSource>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT LocationTracker : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool naming READ naming NOTIFY stateChanged)
    Q_PROPERTY(bool hasPosition READ hasPosition NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QString placeName READ placeName NOTIFY stateChanged)
public:
    explicit LocationTracker(QObject *parent = nullptr, QGeoPositionInfoSource *source = nullptr, int lookupTimeoutMs = 15000);
    bool enabled() const { return m_enabled; }
    bool busy() const { return m_busy; }
    bool naming() const { return m_geocoder.busy(); }
    bool hasPosition() const { return m_coordinate.isValid(); }
    QString error() const { return m_error; }
    QString placeName() const { return m_placeName; }
    void setEnabled(bool enabled);
    void setForeground(bool foreground);
    Q_INVOKABLE void retry();
signals:
    void stateChanged();
    void positionAccepted(double latitude, double longitude);
private:
    void start();
    void startSource();
    void acceptPosition(const QGeoPositionInfo &position);
    void fail(const QString &error);
    QPointer<QGeoPositionInfoSource> m_source;
    bool m_injectedSource = false;
    bool m_sourceConnected = false;
    bool m_enabled = false, m_foreground = true, m_busy = false, m_permissionPending = false;
    QGeoCoordinate m_coordinate;
    QString m_error, m_placeName;
    ReverseGeocoder m_geocoder;
    QTimer m_lookupDeadline;
};
