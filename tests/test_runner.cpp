#include "MeteoRunner.hpp"
#include "RunnerQuery.hpp"
#include "UserPreferences.hpp"
#include "ServiceFixture.hpp"
#include <KRunner/AbstractRunnerTest>
#include <KRunner/QueryMatch>
#include <KRunner/RunnerContext>
#include <QSettings>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QElapsedTimer>

class RunnerTest : public KRunner::AbstractRunnerTest {
    Q_OBJECT
    QTemporaryDir m_files;
private slots:
    void initTestCase()
    {
        QVERIFY(m_files.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_files.path());
        qputenv("XDG_CACHE_HOME", m_files.path().toUtf8());
        UserPreferences preferences;
        preferences.savePlace("Gdańsk", 54.352, 18.6466);
        preferences.savePlace("Łódź", 51.759, 19.456);
        initProperties();
    }
    void parsesOnlyExplicitWeatherQueries()
    {
        QCOMPARE(Meteo::runnerQuery("METEO   Gdańsk").value(), "Gdańsk");
        QCOMPARE(Meteo::runnerQuery("pogoda").value(), "");
        QVERIFY(!Meteo::runnerQuery("pogodakielce"));
        QVERIFY(!Meteo::runnerQuery("Kielce"));
        QVERIFY(!Meteo::runnerQuery(QString("weather ")+QString(300,'x')));
    }
    void loadsThePluginAndMatchesSavedPlacesWithoutAccents()
    {
        const auto gdansk = launchQuery("pogoda gdansk");
        QCOMPARE(gdansk.size(), 1);
        QCOMPARE(gdansk.first().data().toMap()["name"].toString(), "Gdańsk");
        QCOMPARE(gdansk.first().data().toMap()["model"].toString(), "um4_60");
        const auto lodz = launchQuery("weather lodz");
        QCOMPARE(lodz.size(), 1);
        QCOMPARE(lodz.first().data().toMap()["name"].toString(), "Łódź");
        QCOMPARE(launchQuery("meteo").size(), 2);
        QVERIFY(launchQuery("ordinary unrelated text").isEmpty());
    }
    void searchesGloballyAndReusesThePlaceQueryCache()
    {
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [](const QUrl &) { return QByteArray(R"({"features":[{"geometry":{"coordinates":[-74,40.71]},"properties":{"name":"New York","country":"USA"}}]})"); };
        MeteoRunner remote(nullptr, KPluginMetaData(), server.endpoint());
        KRunner::RunnerContext first; first.setQuery("weather New York");
        remote.match(first);
        QCOMPARE(first.matches().size(), 1);
        QCOMPARE(first.matches().first().data().toMap()["model"].toString(), "gfs");
        QCOMPARE(server.requests.size(), 1);
        QCOMPARE(QUrlQuery(server.requests.first()).queryItemValue("q"), "New York");
        KRunner::RunnerContext second; second.setQuery("weather New York");
        remote.match(second);
        QCOMPARE(second.matches().size(), 1);
        QCOMPARE(server.requests.size(), 1);
    }
    void networkErrorsLeaveTheRunnerResponsive()
    {
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.status = 503; server.response = [](const QUrl &) { return QByteArray("{}"); };
        MeteoRunner remote(nullptr, KPluginMetaData(), server.endpoint());
        KRunner::RunnerContext context; context.setQuery("pogoda Offline test");
        remote.match(context);
        QVERIFY(context.matches().isEmpty());
        QCOMPARE(server.requests.size(), 1);
    }
    void obsoleteQueriesCancelBeforeTheNetworkDeadline()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost)); // Accept no response.
        MeteoRunner remote(nullptr, KPluginMetaData(), QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
        KRunner::RunnerContext context; context.setQuery("weather Slow test");
        QTimer::singleShot(50, &server, [&] { context.setQuery("weather Changed query"); });
        QElapsedTimer elapsed; elapsed.start();
        remote.match(context);
        QVERIFY(elapsed.elapsed() < 1500);
        QVERIFY(context.matches().isEmpty());
    }
    void cleanupTestCase() { manager.reset(); }
};
QTEST_MAIN(RunnerTest)
#include "test_runner.moc"
