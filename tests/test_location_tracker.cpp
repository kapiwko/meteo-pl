#include "LocationTracker.hpp"
#include <QTest>
#include <QSignalSpy>

#include "PositionFixture.hpp"

class LocationTrackerTest : public QObject {
    Q_OBJECT
private slots:
    void tracksOnlyInForegroundAndIgnoresJitterAndOldPositions()
    {
        auto *source = new PositionFixture;
        LocationTracker tracker(nullptr, source);
        QSignalSpy positions(&tracker, &LocationTracker::positionAccepted);
        QCOMPARE(source->starts, 0);
        tracker.setEnabled(true);
        QVERIFY(tracker.busy()); QCOMPARE(source->starts, 1);
        source->send(50.86, 20.63, 3600);
        QVERIFY(tracker.busy()); QVERIFY(!tracker.hasPosition());
        source->send(50.86, 20.63);
        QVERIFY(!tracker.busy()); QVERIFY(tracker.hasPosition()); QCOMPARE(positions.size(), 1);
        source->send(50.8601, 20.6301);
        QCOMPARE(positions.size(), 1);
        source->send(50.9, 20.7);
        QCOMPARE(positions.size(), 2);
        tracker.setForeground(false);
        const auto stops = source->stops;
        source->send(51.1, 20.7);
        QCOMPARE(positions.size(), 2);
        tracker.setForeground(true);
        QCOMPARE(source->starts, 2); QVERIFY(tracker.busy());
        source->send(51.1, 20.7);
        QCOMPARE(positions.size(), 3); QVERIFY(!tracker.busy());
        emit source->errorOccurred(QGeoPositionInfoSource::UpdateTimeoutError);
        QVERIFY(!tracker.error().isEmpty()); QVERIFY(tracker.hasPosition());
        tracker.retry(); QVERIFY(tracker.error().isEmpty());
        tracker.setEnabled(false);
        QVERIFY(source->stops > stops); QVERIFY(!tracker.hasPosition());
        source->send(51.2, 20.7);
        QCOMPARE(positions.size(), 3);
        tracker.setEnabled(true);
        source->send(51.2, 20.7);
        QCOMPARE(positions.size(), 4);
    }
};
QTEST_GUILESS_MAIN(LocationTrackerTest)
#include "test_location_tracker.moc"
