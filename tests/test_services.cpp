#include "PlaceSearchModel.hpp"
#include "WeatherLayersModel.hpp"
#include "EditorialModel.hpp"
#include "LegacyMeteogramModel.hpp"
#include "UserPreferences.hpp"
#include "ChartConfiguration.hpp"
#include "core/Locations.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
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
    QTemporaryDir m_config;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_config.path());
    }
    void init()
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "meteo-pl", "desktop");
        settings.clear();
        settings.sync();
    }
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
        QVERIFY(preferences.showLegend());
        preferences.setShowLegend(false);
        QCOMPARE(preferences.probeMode(), "chart");
        preferences.setProbeMode("all");
        UserPreferences probeReload;
        QCOMPARE(probeReload.probeMode(), "all");
        probeReload.setProbeMode("invalid");
        QCOMPARE(probeReload.probeMode(), "all");
        probeReload.setProbeMode("off");
        QCOMPARE(preferences.probeMode(), "off");
        probeReload.setProbeMode("chart");
        QCOMPARE(preferences.renderer(), "gpu");
        preferences.setRenderer("cpu");
        UserPreferences rendererReload;
        QCOMPARE(rendererReload.renderer(), "cpu");
        rendererReload.setRenderer("invalid");
        QCOMPARE(rendererReload.renderer(), "cpu");
        rendererReload.setRenderer("gpu");
        QCOMPARE(preferences.visibleGroups(), QStringList({"temperature", "precipitation", "probability", "pressure", "wind", "windDirection", "cloudHeights", "clouds"}));
        const auto id = preferences.createProfile(QStringLiteral("Moje góry"));
        QVERIFY(id.startsWith("user:"));
        preferences.moveChart(4, -4);
        preferences.setChartField("temperature", "dwptmp_point", false);
        preferences.setChartEnabled("probability", false);
        UserPreferences reopened;
        QCOMPARE(reopened.profile(), id);
        QVERIFY(!reopened.showLegend());
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
    void arbitraryChartSeriesAndAppearancePersistPerProfile()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        UserPreferences preferences;
        const auto profile = preferences.createProfile("Temperatura i deszcz");
        const auto chart = preferences.createChart("Mój wykres");
        QVERIFY(chart.startsWith("chart:"));
        preferences.setChartField(chart, "airtmp_point", true);
        preferences.setChartField(chart, "pcpttl_aver", true);
        preferences.setSeriesStyle(chart, "pcpttl_aver", {{"axis", "right"}, {"color", "#123456"}, {"darkColor", "#abcdef"}, {"render", "area"}, {"opacity", 0.35}, {"lineWidth", 3.5}, {"lineStyle", "dashed"}, {"layer", 42}});
        preferences.setChartOptions(chart, {{"title", "Temperatura + opad"}, {"leftMin", -20}, {"leftMax", 40}, {"rightMax", 10}});
        UserPreferences reopened;
        QCOMPARE(reopened.profile(), profile);
        const auto saved = reopened.chartLayout().last().toMap();
        QCOMPARE(saved["key"], chart);
        QCOMPARE(saved["title"].toString(), "Temperatura + opad");
        QCOMPARE(saved["leftMin"].toDouble(), -20);
        QCOMPARE(saved["rightMax"].toDouble(), 10);
        QCOMPARE(saved["fields"].toStringList(), QStringList({"airtmp_point", "pcpttl_aver"}));
        const auto appearance = saved["series"].toList().last().toMap();
        QCOMPARE(appearance["axis"].toString(), "right");
        QCOMPARE(appearance["color"].toString(), "#123456");
        QCOMPARE(appearance["darkColor"].toString(), "#abcdef");
        QCOMPARE(appearance["render"].toString(), "area");
        QCOMPARE(appearance["opacity"].toDouble(), 0.35);
        QCOMPARE(appearance["lineWidth"].toDouble(), 3.5);
        QCOMPARE(appearance["lineStyle"].toString(), "dashed");
        QCOMPARE(appearance["layer"].toInt(), 42);
        reopened.setProfile("classic");
        QVERIFY(reopened.chartLayout().last().toMap()["key"] != chart);
        reopened.setProfile(profile);
        reopened.setChartField(chart, "pcpttl_aver", false);
        reopened.setChartField(chart, "pcpttl_aver", true);
        QCOMPARE(reopened.chartLayout().last().toMap()["series"].toList().last().toMap()["color"].toString(), "#123456");
        reopened.removeSeries(chart, "airtmp_point");
        QCOMPARE(reopened.chartLayout().last().toMap()["fields"].toStringList(), QStringList{"pcpttl_aver"});
        reopened.deleteChart(chart);
        QVERIFY(reopened.chartLayout().last().toMap()["key"] != chart);
    }
    void profileFilesRoundTripAppearanceAndNeverOverwriteExistingProfiles()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        UserPreferences preferences;
        QVERIFY(preferences.settingsFile().endsWith("/meteo-pl/desktop.ini"));
        const auto originalId = preferences.createProfile(QStringLiteral("Góry zimą"));
        preferences.setChartHeight(220);
        preferences.setProfileChartHeight(260);
        const auto chart = preferences.createChart("Temperatura + deszcz");
        preferences.setChartField(chart, "airtmp_point", true);
        preferences.setChartField(chart, "pcpttl_aver", true);
        preferences.setSeriesStyle(chart, "pcpttl_aver", {{"axis", "right"}, {"color", "#123456"}, {"opacity", 0.4}, {"render", "stems"}});
        preferences.setChartOptions(chart, {{"leftMin", -20}, {"leftMax", 35}, {"rightMax", 10}, {"height",190}});
        preferences.moveChart(preferences.chartLayout().size() - 1, -(preferences.chartLayout().size() - 1));
        const auto original = preferences.chartLayout();
        const auto file = QUrl::fromLocalFile(config.filePath("profil ze spacjami ż.json"));
        const auto exported = preferences.exportProfiles(file);
        QVERIFY2(exported["ok"].toBool(), qPrintable(exported["error"].toString()));
        QCOMPARE(exported["count"].toInt(), 1);
        QFile json(file.toLocalFile());
        QVERIFY(json.open(QIODevice::ReadOnly));
        auto document = QJsonDocument::fromJson(json.readAll()).object();
        QCOMPARE(document["version"].toInt(), 1);
        QVERIFY(!document["profiles"].toArray().first().toObject().contains("id"));
        const auto imported = preferences.importProfiles(file);
        QVERIFY2(imported["ok"].toBool(), qPrintable(imported["error"].toString()));
        QCOMPARE(preferences.chartLayout(), original);
        QCOMPARE(preferences.profileChartHeight(),260);
        QCOMPARE(preferences.chartHeight(),220);
        QCOMPARE(preferences.chartLayout().first().toMap()["height"].toInt(),190);
        QVERIFY(preferences.profile() != originalId);
        QCOMPARE(preferences.profiles().last().toMap()["name"].toString(), QStringLiteral("Góry zimą (2)"));
        preferences.setProfile(originalId);
        QCOMPARE(preferences.chartLayout(), original);
        preferences.setChartField("temperature", "dwptmp_point", false); // This edit must stay in the original profile.
        const auto all = QUrl::fromLocalFile(config.filePath("wszystkie.json"));
        QCOMPARE(preferences.exportProfiles(all, true)["count"].toInt(), 2);
        QCOMPARE(preferences.importProfiles(all)["count"].toInt(), 2);
        QCOMPARE(preferences.profiles().last().toMap()["name"].toString(), QStringLiteral("Góry zimą (2) (2)"));
        UserPreferences reopened;
        QCOMPARE(reopened.profiles().size(), 10); // Six built-ins, four independent saved profiles.
        QCOMPARE(reopened.profile(), preferences.profile());
        QCOMPARE(reopened.chartLayout(), preferences.chartLayout());
    }
    void malformedProfileImportsLeaveAllPreferencesUntouched()
    {
        QTemporaryDir config;
        QVERIFY(config.isValid());
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        UserPreferences preferences;
        preferences.createProfile("Zachowaj mnie");
        const auto id = preferences.profile();
        const auto original = preferences.chartLayout();
        const auto originalProfiles = preferences.profiles();
        const auto file = QUrl::fromLocalFile(config.filePath("bad.json"));
        const auto validProfile = QJsonObject{{"name", "Dobry"}, {"charts", QJsonArray::fromVariantList(original)}};
        const auto badProfile = QJsonObject{{"name", "Błędny"}, {"charts", QJsonArray{QJsonObject{{"key", "chart:test"},
            {"series", QJsonArray{QJsonObject{{"key", "nonexistent"}}}}}}}};
        const QList<QByteArray> invalid{
            "{broken", "[]",
            QJsonDocument(QJsonObject{{"format", "unknown"}, {"version", 1}, {"profiles", QJsonArray{validProfile}}}).toJson(),
            QJsonDocument(QJsonObject{{"format", "pl.meteo.chart-profiles"}, {"version", 2}, {"profiles", QJsonArray{validProfile}}}).toJson(),
            QJsonDocument(QJsonObject{{"format", "pl.meteo.chart-profiles"}, {"version", 1}, {"profiles", QJsonArray{validProfile, badProfile}}}).toJson()
        };
        for (const auto &bytes : invalid) {
            QFile output(file.toLocalFile());
            QVERIFY(output.open(QIODevice::WriteOnly));
            QCOMPARE(output.write(bytes), bytes.size());
            output.close();
            const auto result = preferences.importProfiles(file);
            QVERIFY(!result["ok"].toBool());
            QVERIFY(!result["error"].toString().isEmpty());
            QCOMPARE(preferences.profile(), id);
            QCOMPARE(preferences.chartLayout(), original);
            QCOMPARE(preferences.profiles(), originalProfiles);
        }
        QVERIFY(!preferences.importProfiles(QUrl("https://example.com/profiles.json"))["ok"].toBool());
        QVERIFY(!preferences.exportProfiles(QUrl::fromLocalFile(config.filePath("missing/profiles.json")))["ok"].toBool());
    }
    void oldLayoutsMigrateAndInvalidStylesAreRejected()
    {
        const QVariantList legacy{QVariantMap{{"key", "temperature"}, {"enabled", true},
            {"fields", QStringList{"airtmp_min", "airtmp_max", "slpres_point"}}}};
        const auto migrated = normalizeChartLayout(legacy);
        QCOMPARE(migrated.size(), 1);
        const auto series = migrated.first().toMap()["series"].toList();
        QCOMPARE(series.size(), 2);
        QCOMPARE(series.first().toMap()["render"].toString(), "range");
        QCOMPARE(series.last().toMap()["key"].toString(), "slpres_point");
        QVERIFY(normalizeSeriesStyle({{"key", "unknown"}}).isEmpty());
        const auto safe = normalizeSeriesStyle({{"key", "airtmp_point"}, {"color", "javascript:bad"}, {"render", "script"},
            {"lineWidth", 9999}, {"opacity", -3}, {"axis", "unknown"}});
        QCOMPARE(safe["color"].toString(), "#f44336");
        QCOMPARE(safe["render"].toString(), "line");
        QCOMPARE(safe["axis"].toString(), "auto");
        QCOMPARE(safe["lineWidth"].toDouble(), 8.0);
        QCOMPARE(safe["opacity"].toDouble(), 0.0);
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
