#include "core/SolarCalendar.hpp"
#include <QDateTime>
#include <QTest>
#include <tuple>
#include <utility>

class SolarTest : public QObject {
    Q_OBJECT
private slots:
    void geographicNight_data()
    {
        QTest::addColumn<QString>("time"); QTest::addColumn<double>("latitude");
        QTest::addColumn<double>("longitude"); QTest::addColumn<bool>("night");
        QTest::newRow("northern polar day") << QString("2026-06-21T00:00:00Z") << 69.65 << 18.96 << false;
        QTest::newRow("northern polar night") << QString("2026-12-21T12:00:00Z") << 69.65 << 18.96 << true;
        QTest::newRow("southern polar day") << QString("2026-12-21T00:00:00Z") << -75.0 << 0.0 << false;
        QTest::newRow("southern polar night") << QString("2026-06-21T12:00:00Z") << -75.0 << 0.0 << true;
        QTest::newRow("winter morning") << QString("2026-01-01T04:00:00Z") << 50.8661 << 20.6286 << true;
        QTest::newRow("summer morning") << QString("2026-06-21T04:00:00Z") << 50.8661 << 20.6286 << false;
        QTest::newRow("UTC midnight east") << QString("2026-06-21T00:00:00Z") << 0.0 << 180.0 << false;
        QTest::newRow("UTC midnight Greenwich") << QString("2026-06-21T00:00:00Z") << 0.0 << 0.0 << true;
    }
    void geographicNight()
    {
        QFETCH(QString, time); QFETCH(double, latitude); QFETCH(double, longitude); QFETCH(bool, night);
        QCOMPARE(Meteo::nightAt(QDateTime::fromString(time, Qt::ISODate).toSecsSinceEpoch(), latitude, longitude), night);
    }
    void eventsAndBackgroundShareTheSameHorizon()
    {
        const QTimeZone zone("Europe/Warsaw");
        const QDate date(2026,10,25);
        const auto events = Meteo::solarEvents(date, zone, 50.8661, 20.6286);
        QVERIFY(events.sunrise); QVERIFY(events.sunset);
        const auto sunrise = *events.sunrise, sunset = *events.sunset;
        QVERIFY(Meteo::nightAt(sunrise-60, 50.8661, 20.6286));
        QVERIFY(!Meteo::nightAt(sunrise, 50.8661, 20.6286));
        QVERIFY(!Meteo::nightAt(sunset-60, 50.8661, 20.6286));
        QVERIFY(Meteo::nightAt(sunset, 50.8661, 20.6286));
        const auto nights = Meteo::nightsBetween(date.startOfDay(zone).toSecsSinceEpoch(), date.addDays(1).startOfDay(zone).toSecsSinceEpoch(), 50.8661, 20.6286);
        QCOMPARE(nights.size(), 2);
        QVERIFY(qAbs(nights[0].end - sunrise) <= 900);
        QVERIFY(qAbs(nights[1].start - sunset) <= 900);
        for (const auto &[latitude, day, night] : {std::tuple{69.65,QDate(2026,6,21),false}, std::tuple{-75.0,QDate(2026,6,21),true}}) {
            const auto polar = Meteo::solarEvents(day, QTimeZone::utc(), latitude, 0);
            QVERIFY(!polar.sunrise); QVERIFY(!polar.sunset);
            QCOMPARE(polar.polarNight, night);
        }
    }
    void calendarUsesElapsedTimeAcrossDst()
    {
        const QTimeZone zone("Europe/Warsaw");
        for (const auto &[day, hours] : {std::pair{QDate(2026,3,29),23}, std::pair{QDate(2026,10,25),25}}) {
            const auto from = day.startOfDay(zone).toSecsSinceEpoch(), to = day.addDays(1).startOfDay(zone).toSecsSinceEpoch();
            const auto spans = Meteo::calendarSpans(from, to, zone);
            QCOMPARE(spans.size(), 1);
            QCOMPARE(spans[0].end-spans[0].start, hours*3600);
        }
    }
};
QTEST_GUILESS_MAIN(SolarTest)
#include "test_solar.moc"
