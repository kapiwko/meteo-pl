#include "ClientFixture.hpp"
#include "data/ForecastRepository.hpp"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class RepositoryTest : public QObject
{
    Q_OBJECT
    QTemporaryDir m_cache;
private slots:
    void initTestCase()
    {
        QVERIFY(m_cache.isValid());
        qputenv("XDG_CACHE_HOME", m_cache.path().toUtf8());
    }
    void persistsAValidatedSnapshotForTheSameCanonicalApiPoint()
    {
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const Meteo::ForecastQuery query{50.866123456789, 20.628612345678, "um4_60", 0};
        QVERIFY(query.valid());
        Meteo::ForecastSnapshot downloaded;
        {
            Meteo::ForecastRepository repository(nullptr, server.endpoint());
            QSignalSpy received(&repository, &Meteo::ForecastRepository::received);
            QSignalSpy failed(&repository, &Meteo::ForecastRepository::failed);
            repository.fetch(query);
            QTRY_COMPARE(received.count(), 1);
            QCOMPARE(failed.count(), 0);
            downloaded = qvariant_cast<Meteo::ForecastSnapshot>(received.first().first());
            QVERIFY(!downloaded.usingCache);
            QVERIFY(downloaded.fetchedAt.isValid());
            QVERIFY(downloaded.forecast.modelRun.isValid());
            QVERIFY(!downloaded.sourceRevision.isEmpty());
            QCOMPARE(downloaded.query.latitude, 50.866123);
            QCOMPARE(downloaded.query.longitude, 20.628612);
        }
        Meteo::ForecastRepository reopened(nullptr, server.endpoint());
        const auto cached = reopened.cached({50.8661234, 20.6286123, "um4_60", 0});
        QVERIFY(cached);
        QVERIFY(cached->usingCache);
        QCOMPARE(cached->sourceRevision, downloaded.sourceRevision);
        QCOMPARE(cached->forecast.modelRun, downloaded.forecast.modelRun);
        QCOMPARE(server.requests.size(), 2);
        QVERIFY(!reopened.cached({50.866125, 20.6286123, "um4_60", 0}));
        QVERIFY(!reopened.cached({50.8661234, 20.6286123, "um4_60", 300}));
        server.forecast = "{}";
        QSignalSpy failed(&reopened, &Meteo::ForecastRepository::failed);
        reopened.fetch(query);
        QTRY_COMPARE(failed.count(), 1);
        const auto retained=reopened.cached(query);
        QVERIFY(retained);
        QCOMPARE(retained->sourceRevision, downloaded.sourceRevision);
    }
    void invalidQueriesNeverStartNetworkRequests()
    {
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Meteo::ForecastRepository repository(nullptr, server.endpoint());
        QSignalSpy failed(&repository, &Meteo::ForecastRepository::failed);
        repository.fetch({52, 21, "unknown", 0});
        repository.fetch({91, 21, "gfs", 0});
        repository.fetch({52, 21, "um4_60", 253402300800LL});
        QCOMPARE(failed.count(), 3);
        QTest::qWait(50);
        QVERIFY(server.requests.isEmpty());
    }
};
QTEST_GUILESS_MAIN(RepositoryTest)
#include "test_repository.moc"
