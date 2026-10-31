#pragma once
#include <QGeoPositionInfoSource>
#include <QDateTime>

class PositionFixture : public QGeoPositionInfoSource {
public:
    PositionFixture() : QGeoPositionInfoSource(nullptr) {}
    int starts = 0, stops = 0;
    QGeoPositionInfo lastKnownPosition(bool = false) const override { return {}; }
    PositioningMethods supportedPositioningMethods() const override { return AllPositioningMethods; }
    int minimumUpdateInterval() const override { return 1000; }
    Error error() const override { return NoError; }
    void startUpdates() override { ++starts; }
    void stopUpdates() override { ++stops; }
    void requestUpdate(int) override {}
    void send(double lat, double lon, int secondsAgo = 0) {
        emit positionUpdated(QGeoPositionInfo(QGeoCoordinate(lat, lon), QDateTime::currentDateTimeUtc().addSecs(-secondsAgo)));
    }
};
