#include "PlaceSearchModel.hpp"
#include "WeatherLayersModel.hpp"
#include "EditorialModel.hpp"
#include "LegacyMeteogramModel.hpp"
#include "UserPreferences.hpp"
#include "core/Locations.hpp"
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
#include <functional>

#include "ServiceFixture.hpp"

class ServicesTest : public QObject {
    Q_OBJECT
private slots:
    void parsesVillagesAndDisambiguatesRegions()
    {
        const auto document = QJsonDocument::fromJson(R"([{"city":"Nowa Wieś","lat":"50.123456","lon":20.1,"tags":{"state":"świętokrzyskie","country":"Polska"}},{"city":"Nowa Wieś","lat":51.3,"lon":21.1,"tags":{"state":"mazowieckie"}},{"city":"Invalid","lat":"bad","lon":20}])");
        const auto places = Meteo::parseLocations(document);
        QCOMPARE(places.size(), 2);
        QCOMPARE(places[0].toMap()["latitude"].toDouble(), 50.123456);
        QVERIFY(places[0].toMap()["description"].toString().contains(QStringLiteral("świętokrzyskie")));
    }
    void searchEncodesPolishNamesAndClearsOldResults()
    {
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [](const QUrl &) { return QByteArray(R"([{"city":"Chęciny","lat":50.8,"lon":20.4,"tags":{"state":"świętokrzyskie"}}])"); };
        PlaceSearchModel model(nullptr, server.endpoint().resolved(QUrl("/lookup")), server.endpoint().resolved(QUrl("/photon")));
        model.setQuery(QStringLiteral("Chęciny"));
        model.search();
        QTRY_VERIFY(!model.busy());
        QCOMPARE(model.results().size(), 1);
        const QUrlQuery query(server.requests.last());
        QCOMPARE(query.queryItemValue("term"), QStringLiteral("Chęciny"));
        QCOMPARE(query.queryItemValue("country"), "pl");
        model.setQuery("London");
        QVERIFY(model.results().isEmpty());
        model.setPolandOnly(false);
        model.search();
        QTRY_VERIFY(!model.busy());
        QVERIFY(!QUrlQuery(server.requests.last()).hasQueryItem("country"));
        QCOMPARE(QUrlQuery(server.requests.last()).queryItemValue("q"), "London");
        model.setQuery("");
        QVERIFY(!model.busy());
        QVERIFY(model.results().isEmpty());
    }
    void weatherMapsUsePublishedRunAndForecastStep()
    {
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [](const QUrl &) { return QByteArray(R"({"dates":[{"date":"20261024T00:00:00Z","firstStep":1,"lastStep":120},{"date":"20261025T06:00:00Z","firstStep":1,"lastStep":60}]})"); };
        WeatherLayersModel model(nullptr, server.endpoint());
        model.setActive(true);
        QTRY_VERIFY(!model.busy());
        QCOMPARE(model.runs().size(), 2);
        QCOMPARE(model.lastStep(), 60);
        model.setStep(12);
        const QUrlQuery tile(model.tileUrl(35, 22, 6));
        QCOMPARE(tile.queryItemValue("TIME"), "2026-10-25T06:00:00Z");
        QCOMPARE(tile.queryItemValue("DIM_FORECAST"), "12");
        QCOMPARE(tile.queryItemValue("srs"), "EPSG:3857");
        QVERIFY(tile.queryItemValue("bbox").contains(','));
        model.setRunIndex(1);
        QCOMPARE(model.lastStep(), 120);
        model.setStep(1000);
        QCOMPARE(model.step(), 120);
    }
    void editorialSupportsArchivesAndPagination()
    {
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [](const QUrl &url) {
            if (url.path().contains("synoptic-comment")) return QByteArray(R"({"content":"<p>Test commentary</p>","published_date":"2026-10-24"})");
            return QByteArray(R"([{"title":"Test article","content":"<p>Content</p>","link":"/test","date_gmt":"2026-10-25T00:00:00"}])");
        };
        EditorialModel model(nullptr, server.endpoint());
        model.setDate("2026-10-24");
        QTRY_VERIFY(!model.busy());
        QVERIFY(server.requests.last().path().endsWith("/synoptic-comment/2026-10-24/pl"));
        QVERIFY(model.commentHtml().contains("Test commentary"));
        model.setCommentary(false);
        model.reload();
        QTRY_VERIFY(!model.busy());
        QCOMPARE(model.articles().size(), 1);
        model.more();
        QTRY_VERIFY(!model.busy());
        QCOMPARE(QUrlQuery(server.requests.last()).queryItemValue("page"), "2");
    }
    void editorialRejectsFutureDatesAndExplainsMissingComments()
    {
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [](const QUrl &) { return QByteArray(R"({"content":"<p>Comment</p>","published_date":"2026-10-25"})"); };
        EditorialModel model(nullptr, server.endpoint());
        model.setDate(QDate::currentDate().addDays(1).toString(Qt::ISODate));
        QVERIFY(!model.busy());
        QVERIFY(model.date().isEmpty());
        QVERIFY(!model.notice().isEmpty());
        QCOMPARE(server.requests.size(), 0);
        server.status = 404;
        const auto missingDate = QDate::currentDate().addDays(-1).toString(Qt::ISODate);
        model.setDate(missingDate);
        QTRY_VERIFY(!model.busy());
        QVERIFY(model.error().isEmpty());
        QVERIFY(model.notice().contains(missingDate));
        QVERIFY(model.commentHtml().isEmpty());
        server.status = 200;
        model.setDate("");
        QTRY_VERIFY(!model.busy());
        QVERIFY(model.notice().isEmpty());
        QVERIFY(model.error().isEmpty());
        QVERIFY(model.commentHtml().contains("Comment"));
    }
    void originalMeteogramUsesCoordinatesReturnedByTheOfficialSelector()
    {
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [](const QUrl &) { return QByteArray(); };
        server.status = 301;
        server.redirect = "./meteorogram_map_um.php?ntype=0u&row=444&col=241&lang=pl";
        LegacyMeteogramModel model(nullptr, server.endpoint());
        model.setLookup(server.endpoint().resolved(QUrl("/um/php/mgram_search.php?NALL=50.8661&EALL=20.6286&fdate=2026102506")));
        QTRY_VERIFY(!model.busy());
        QVERIFY(model.error().isEmpty());
        QCOMPARE(model.source().path(), "/um/metco/mgram_pict.php");
        const QUrlQuery query(model.source());
        QCOMPARE(query.queryItemValue("row"), "444");
        QCOMPARE(query.queryItemValue("col"), "241");
        QCOMPARE(query.queryItemValue("fdate"), "2026102506");
        QCOMPARE(server.requests.size(), 1);
        server.status = 404;
        server.redirect.clear();
        model.reload();
        QTRY_VERIFY(!model.busy());
        QVERIFY(!model.error().isEmpty());
    }
    void namedProfilesPersistChartOrderAndSelectedFields()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        UserPreferences preferences;
        QCOMPARE(preferences.profile(), "classic");
        QCOMPARE(preferences.visibleGroups(), QStringList({"temperature", "precipitation", "probability", "pressure", "wind", "windDirection", "cloudHeights", "clouds"}));
        const auto id = preferences.createProfile(QStringLiteral("Moje góry"));
        QVERIFY(id.startsWith("user:"));
        preferences.moveChart(4, -4);
        preferences.setChartField("temperature", "dwptmp_point", false);
        preferences.setChartEnabled("probability", false);
        UserPreferences reopened;
        QCOMPARE(reopened.profile(), id);
        QCOMPARE(reopened.chartLayout().first().toMap()["key"].toString(), "wind");
        QVERIFY(!reopened.visibleGroups().contains("probability"));
        const auto hasDew = [](const QVariantList &layout) {
            for (const auto &entry : layout) if (entry.toMap()["key"] == "temperature") return entry.toMap()["fields"].toStringList().contains("dwptmp_point");
            return false;
        };
        QVERIFY(!hasDew(reopened.chartLayout()));
        reopened.renameProfile("Góry zimą");
        QCOMPARE(reopened.profiles().last().toMap()["name"].toString(), QStringLiteral("Góry zimą"));
        reopened.setProfile("classic");
        QVERIFY(hasDew(reopened.chartLayout()));
        QVERIFY(reopened.visibleGroups().contains("probability"));
        reopened.setProfile(id);
        reopened.deleteProfile();
        QCOMPARE(reopened.profile(), "classic");
        QVERIFY(!reopened.profiles().last().toMap()["id"].toString().startsWith("user:"));
    }
    void favoritesPersistAndRecentPlacesAreDeduplicated()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        UserPreferences preferences;
        preferences.toggleFavorite("Chęciny", 50.8, 20.4);
        QVERIFY(preferences.isFavorite(50.8, 20.4));
        UserPreferences second;
        QCOMPARE(second.favorites().size(), 1);
        preferences.rememberPlace("First", 50.8, 20.4);
        preferences.rememberPlace("Renamed", 50.8, 20.4);
        QCOMPARE(preferences.recentPlaces().size(), 1);
        QCOMPARE(preferences.recentPlaces().first().toMap()["name"].toString(), "Renamed");
        preferences.toggleFavorite("Chęciny", 50.8, 20.4);
        QVERIFY(!preferences.isFavorite(50.8, 20.4));
    }
};
QTEST_GUILESS_MAIN(ServicesTest)
#include "test_services.moc"
