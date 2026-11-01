#include "LocationTracker.hpp"
#include <QTest>
#include <QSignalSpy>

#include "PositionFixture.hpp"

class LocationTrackerTest : public QObject {
    Q_OBJECT
private slots:
    void systemPermissionRequestsWaitUntilTheCallingCallbackReturns()
    {
        auto *source=new PositionFixture;
        bool insideCallback=true;
        LocationTracker tracker(nullptr,source,100,[&](QObject *,LocationTracker::PermissionResult completed) {
            QVERIFY(!insideCallback);
            completed(Qt::PermissionStatus::Granted);
        });
        tracker.setEnabled(true);
        QVERIFY(tracker.busy()); QCOMPARE(source->starts,0);
        insideCallback=false;
        QTRY_COMPARE(source->starts,1);
        source->send(50.86,20.63); QVERIFY(tracker.hasPosition());
    }
    void oneShotStopsAfterItsFirstFreshFixAndDoesNotRestartOnResume()
    {
        auto *source=new PositionFixture;
        LocationTracker tracker(nullptr,source);
        tracker.setOneShot(true);
        QSignalSpy positions(&tracker,&LocationTracker::positionAccepted);
        tracker.setEnabled(true);
        source->send(50.86,20.63,3600);
        QVERIFY(tracker.busy());
        const auto stops=source->stops;
        source->send(50.86,20.63);
        QVERIFY(tracker.hasPosition()); QVERIFY(!tracker.busy()); QVERIFY(source->stops>stops);
        source->send(54.35,18.65); QCOMPARE(positions.size(),1);
        tracker.setForeground(false); tracker.setForeground(true); QCOMPARE(source->starts,1);
        tracker.setEnabled(false); tracker.setEnabled(true);
        source->send(54.35,18.65); QCOMPARE(positions.size(),2);
    }
    void permissionGrantedAfterReturningFromBackgroundRestoresActivity_data()
    {
        QTest::addColumn<bool>("positionArrives");
        QTest::newRow("valid fix") << true;
        QTest::newRow("silent provider times out") << false;
    }
    void permissionGrantedAfterReturningFromBackgroundRestoresActivity()
    {
        QFETCH(bool, positionArrives);
        auto *source = new PositionFixture;
        LocationTracker::PermissionResult complete;
        int requests = 0;
        LocationTracker tracker(nullptr, source, 100, [&](QObject *context, LocationTracker::PermissionResult callback) {
            QVERIFY(context); ++requests; complete = std::move(callback);
        });
        tracker.setEnabled(true);
        QVERIFY(tracker.busy()); QTRY_COMPARE(requests, 1); QCOMPARE(source->starts, 0);
        tracker.setForeground(false);
        QVERIFY(!tracker.busy());
        QSignalSpy changed(&tracker, &LocationTracker::stateChanged);
        tracker.setForeground(true);
        QVERIFY(tracker.busy());
        QVERIFY(!changed.isEmpty()); // Resume activity even while the system prompt is pending.
        QCOMPARE(requests, 1); // The system permission prompt is still pending.
        complete(Qt::PermissionStatus::Granted);
        QCOMPARE(source->starts, 1);
        QVERIFY(tracker.busy());
        if (positionArrives) {
            source->send(50.86, 20.63);
            QVERIFY(tracker.hasPosition()); QVERIFY(!tracker.busy());
            QVERIFY(tracker.error().isEmpty());
        } else {
            QTRY_VERIFY(!tracker.busy());
            QVERIFY(!tracker.error().isEmpty());
        }
    }
    void latePermissionResultDoesNotStartTracking_data()
    {
        QTest::addColumn<bool>("disabled");
        QTest::newRow("disabled") << true;
        QTest::newRow("in background") << false;
    }
    void latePermissionResultDoesNotStartTracking()
    {
        QFETCH(bool, disabled);
        auto *source = new PositionFixture;
        LocationTracker::PermissionResult complete;
        LocationTracker tracker(nullptr, source, 30, [&](QObject *, LocationTracker::PermissionResult callback) { complete = std::move(callback); });
        tracker.setEnabled(true); QTRY_VERIFY(bool(complete));
        if (disabled) tracker.setEnabled(false); else tracker.setForeground(false);
        complete(Qt::PermissionStatus::Granted);
        QCOMPARE(source->starts, 0); QVERIFY(!tracker.busy());
        QTest::qWait(60); QVERIFY(tracker.error().isEmpty());
    }
    void permissionDeniedAfterReturningFromBackgroundOffersRetry()
    {
        auto *source = new PositionFixture;
        LocationTracker::PermissionResult complete;
        LocationTracker tracker(nullptr, source, 30, [&](QObject *, LocationTracker::PermissionResult callback) { complete = std::move(callback); });
        tracker.setEnabled(true); QTRY_VERIFY(bool(complete)); tracker.setForeground(false); tracker.setForeground(true);
        complete(Qt::PermissionStatus::Denied);
        QVERIFY(!tracker.busy()); QVERIFY(!tracker.error().isEmpty()); QCOMPARE(source->starts, 0);
        complete={}; tracker.retry(); QVERIFY(tracker.busy()); QVERIFY(tracker.error().isEmpty());
        QTRY_VERIFY(bool(complete)); complete(Qt::PermissionStatus::Granted);
        QCOMPARE(source->starts, 1);
        source->send(50.86, 20.63);
        QVERIFY(tracker.hasPosition()); QVERIFY(!tracker.busy()); QVERIFY(tracker.error().isEmpty());
    }
    void silentProviderTimesOutAndAValidFixCancelsTheDeadline()
    {
        auto *source = new PositionFixture;
        LocationTracker tracker(nullptr, source, 30);
        tracker.setEnabled(true); QVERIFY(tracker.busy());
        source->send(50.86, 20.63, 3600); // A stale fix must not cancel the deadline.
        QTRY_VERIFY(!tracker.busy());
        QVERIFY(!tracker.error().isEmpty());
        QCOMPARE(source->error(), QGeoPositionInfoSource::NoError);
        tracker.retry(); QVERIFY(tracker.busy());
        source->send(50.86, 20.63);
        QVERIFY(tracker.hasPosition()); QVERIFY(!tracker.busy());
        QTest::qWait(60);
        QVERIFY(tracker.error().isEmpty());
    }
    void backgroundAndDisabledTrackingCancelTheDeadline()
    {
        auto *source = new PositionFixture;
        LocationTracker tracker(nullptr, source, 30);
        tracker.setEnabled(true); tracker.setForeground(false);
        QTest::qWait(60); QVERIFY(tracker.error().isEmpty()); QVERIFY(!tracker.busy());
        tracker.setForeground(true); QVERIFY(tracker.busy());
        tracker.setEnabled(false);
        QTest::qWait(60); QVERIFY(tracker.error().isEmpty()); QVERIFY(!tracker.busy());
    }
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
