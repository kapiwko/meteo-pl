#include "PlaceSearchModel.hpp"
#include "ReverseGeocoder.hpp"
#include "WeatherLayersModel.hpp"
#include "ThematicMapsModel.hpp"
#include "EditorialModel.hpp"
#include "CacheStorage.hpp"
#include "LegacyMeteogramModel.hpp"
#include "UserPreferences.hpp"
#include "ReadingsExporter.hpp"
#include "ApplicationLauncher.hpp"
#include "ChartConfiguration.hpp"
#include "ProfileFileCodec.hpp"
#include "core/Locations.hpp"
#include "core/Localization.hpp"
#include "core/WebLink.hpp"
#include "network/MeteoMapSource.hpp"
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QCryptographicHash>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>
#include <functional>

#include "ServiceFixture.hpp"
#include "WeatherModel.hpp"
#include <QQmlComponent>
#include <QQmlEngine>
#include <ctime>

class ServicesTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
    QTemporaryDir m_cache;
private slots:
    void runLabelsFollowUtcWithoutRefetchingTheCatalog()
    {
        const auto previousZone = qgetenv("TZ");
        const auto restore = qScopeGuard([&] {
            if (previousZone.isNull()) qunsetenv("TZ"); else qputenv("TZ", previousZone);
#ifdef Q_OS_UNIX
            tzset();
#endif
        });
        qputenv("TZ", "Europe/Warsaw");
#ifdef Q_OS_UNIX
        tzset();
#endif
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [](const QUrl &) { return QByteArray(R"({"um4_60":[1792908000,1792886400]})"); };
        WeatherModel weather(nullptr, server.endpoint()); weather.setAutoRefresh(false);
        weather.loadRuns();
        QTRY_COMPARE(weather.availableRuns().size(), 3);
        const auto local = weather.availableRuns();
        const auto timestamp = local[1].toMap()["timestamp"].toLongLong();
        weather.setRequestedRun(timestamp);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQml
            import pl.meteo.desktop
            QtObject {
                required property WeatherModel weather
                readonly property string label: weather.availableRuns.length > 1 ? weather.availableRuns[1].label : ""
            }
        )", {});
        QScopedPointer<QObject> binding(component.createWithInitialProperties({{"weather", QVariant::fromValue(&weather)}}));
        QVERIFY2(binding, qPrintable(component.errorString()));
        QCOMPARE(binding->property("label").toString(), local[1].toMap()["label"].toString());
        QSignalSpy changed(&weather, &WeatherModel::runsChanged);
        const auto requests = server.requests.size();
        weather.setUtc(true);
        QCOMPARE(changed.count(), 1);
        const auto utc = weather.availableRuns();
        QCOMPARE(utc[1].toMap()["timestamp"], local[1].toMap()["timestamp"]);
        QCOMPARE(utc[2].toMap()["timestamp"], local[2].toMap()["timestamp"]);
        QCOMPARE(qint64(weather.requestedRun()), timestamp);
        QCOMPARE(binding->property("label").toString(), weather.formatTime(timestamp, "ddd d MMM · HH:mm t"));
        if (QTimeZone::systemTimeZone().offsetFromUtc(QDateTime::fromSecsSinceEpoch(timestamp)) != 0)
            QVERIFY(binding->property("label").toString() != local[1].toMap()["label"].toString());
        QTest::qWait(50);
        QCOMPARE(server.requests.size(), requests);
    }
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        QVERIFY(m_cache.isValid());
        qputenv("XDG_CACHE_HOME", m_cache.path().toUtf8());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_config.path());
    }
    void init()
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "meteo-pl", "desktop");
        settings.clear();
        settings.sync();
    }
    void opensTheDesktopApplicationWithTheWidgetLocation()
    {
#ifndef Q_OS_UNIX
        QSKIP("The Plasma application handoff runs on Unix.");
#else
        QTemporaryDir files;
        QVERIFY(files.isValid());
        const auto output = files.filePath("arguments");
        QFile executable(files.filePath("meteo-pl"));
        QVERIFY(executable.open(QIODevice::WriteOnly));
        executable.write("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"" + output.toUtf8() + "\"\n");
        executable.close();
        QVERIFY(executable.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        const auto oldPath = qgetenv("PATH");
        const auto restorePath = qScopeGuard([&] { qputenv("PATH", oldPath); });
        qputenv("PATH", files.path().toUtf8());
        ApplicationLauncher launcher;
        const auto name = QStringLiteral("Gdańsk \"centrum\"; $(echo test)");
        QVERIFY(launcher.openForecast(name, 54.352, 18.6466, "um4_120"));
        const auto captured = [&] {
            QFile file(output);
            return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).split('\n') : QStringList{};
        };
        QTRY_COMPARE(captured().size(), 9);
        const auto args = captured();
        QCOMPARE(args[0], "--location"); QCOMPARE(args[1], name);
        QCOMPARE(args[2], "--latitude"); QCOMPARE(args[3].toDouble(), 54.352);
        QCOMPARE(args[4], "--longitude"); QCOMPARE(args[5].toDouble(), 18.6466);
        QCOMPARE(args[6], "--model"); QCOMPARE(args[7], "um4_120");
        QVERIFY(!launcher.openForecast(name, 91, 18, "um4_60"));
        QVERIFY(!launcher.openForecast(name, 54, 18, "unknown"));
        QVERIFY(executable.remove());
        QVERIFY(!launcher.openForecast(name, 54, 18, "um4_60"));
#endif
    }
    void readingCsvPreservesUnitsMissingValuesAndQuotedPolishText()
    {
        QTemporaryDir files;
        QVERIFY(files.isValid());
        ReadingsExporter exporter;
        const auto target = QUrl::fromLocalFile(files.filePath("odczyt"));
        const QVariantList readings{
            QVariantMap{{"key", "wind"}, {"label", "Wiatr; \"średni\"\nna 10 m"}, {"group", "wind"}, {"number", 10.8}, {"unit", "km/h"}, {"value", "10,8 km/h"}},
            QVariantMap{{"key", "missing"}, {"label", "Brak"}, {"group", "temperature"}, {"number", QVariant()}, {"unit", "°C"}, {"value", "—"}}
        };
        const QVariantMap metadata{{"timestamp", 1761397200.0}, {"utc", true}, {"location", "Łódź"}, {"model", "UM4"}, {"run", "run"}, {"scope", "Wszystkie"}, {"source", "ICM UW / meteo.pl"}};
        const auto result = exporter.saveCsv(target, readings, metadata);
        QVERIFY2(result["ok"].toBool(), qPrintable(result["error"].toString()));
        QFile file(result["path"].toString());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll();
        QVERIFY(original.startsWith("\xef\xbb\xbf"));
        const auto text = QString::fromUtf8(original);
        QVERIFY(text.contains("\"Łódź\""));
        QVERIFY(text.contains("\"Wiatr; \"\"średni\"\"\nna 10 m\""));
        QVERIFY(text.contains("\"10.8\";\"km/h\";\"10,8 km/h\""));
        QVERIFY(text.contains("\"Brak\";\"\";\"°C\";\"—\""));
        QVERIFY(text.contains("+00:00") || text.contains("Z\";"));
        QVERIFY(!exporter.saveCsv(result["file"].toUrl(), {}, metadata)["ok"].toBool());
        file.seek(0);
        QCOMPARE(file.readAll(), original);
        QVERIFY(!exporter.saveCsv(QUrl("https://example.org/out.csv"), readings, metadata)["ok"].toBool());
        QVERIFY(!exporter.saveCsv(QUrl::fromLocalFile(files.filePath("missing/out.csv")), readings, metadata)["ok"].toBool());
    }
    void articleLinksShareOneWebNavigationPolicy()
    {
        const QUrl base("https://www.meteo.pl/articles/current");
        QCOMPARE(Meteo::resolveWebLink(QUrl("../other"),base),QUrl("https://www.meteo.pl/other"));
        QCOMPARE(Meteo::resolveWebLink(QUrl("https://example.test/item"),base),QUrl("https://example.test/item"));
        QCOMPARE(Meteo::resolveWebLink(QUrl("//example.test/item"),base),QUrl("https://example.test/item"));
        QCOMPARE(Meteo::resolveWebLink(QUrl("related"),QUrl("/article")),QUrl("https://www.meteo.pl/related"));
        for (const auto &url : {"", "javascript:alert(1)", "file:///tmp/example", "data:text/plain,text", "mailto:test@example.test", "https://"})
            QVERIFY(Meteo::resolveWebLink(QUrl(url),base).isEmpty());
    }
    void parsesVillagesAndDisambiguatesRegions()
    {
        const auto document = QJsonDocument::fromJson(R"([{"city":"Nowa Wieś","lat":"50.123456","lon":20.1,"tags":{"state":"świętokrzyskie","country":"Polska"}},{"city":"Nowa Wieś","lat":51.3,"lon":21.1,"tags":{"state":"mazowieckie"}},{"city":"Invalid","lat":"bad","lon":20}])");
        const auto places = Meteo::parseLocations(document);
        QCOMPARE(places.size(), 2);
        QCOMPARE(places[0].toMap()["latitude"].toDouble(), 50.123456);
        QVERIFY(places[0].toMap()["description"].toString().contains(QStringLiteral("świętokrzyskie")));
    }
    void reverseGeocodingUsesTheCityNameAndCachesTheResult()
    {
        ServiceFixture server; QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [](const QUrl &) { return QByteArray(R"({"features":[{"properties":{"name":"Ogrodowa","city":"Kielce"}}]})"); };
        ReverseGeocoder geocoder(nullptr, server.endpoint().resolved(QUrl("/reverse")));
        QSignalSpy names(&geocoder, &ReverseGeocoder::resolved);
        geocoder.lookup(50.8661, 20.6286); QVERIFY(geocoder.busy());
        QTRY_COMPARE(names.size(), 1);
        QCOMPARE(names.first().first().toString(), "Kielce");
        QVERIFY(!geocoder.busy()); QVERIFY(geocoder.error().isEmpty());
        QCOMPARE(QUrlQuery(server.requests.last()).queryItemValue("lat"), "50.866100");
        QCOMPARE(QUrlQuery(server.requests.last()).queryItemValue("lon"), "20.628600");
        geocoder.lookup(50.8661, 20.6286);
        QTRY_COMPARE(names.size(), 2); QCOMPARE(server.requests.size(), 1);
        geocoder.lookup(1, 2); geocoder.cancel();
        QTest::qWait(100); QCOMPARE(names.size(), 2);
        QVERIFY(!geocoder.busy());
        QVERIFY(ReverseGeocoder::placeName(QJsonDocument::fromJson("{}")).isEmpty());
        QCOMPARE(ReverseGeocoder::placeName(QJsonDocument::fromJson(R"({"features":[{"properties":{"village":"Nowa Wieś","name":"Road"}}]})")), "Nowa Wieś");
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
        const auto count=server.requests.size();
        model.setPolandOnly(true);
        model.setQuery(QStringLiteral("Chęciny"));
        model.search();
        QTRY_VERIFY(!model.busy());
        QCOMPARE(model.results().size(),1);
        QCOMPARE(server.requests.size(),count);
    }
    void metadataCacheHonorsExpiryCancellationAndForcedRefresh()
    {
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response=[](const QUrl &) { return QByteArray(R"({"value":42})"); };
        const auto url=server.endpoint().resolved(QUrl("/cache?model=um4"));
        Meteo::JsonRequest request;
        QSignalSpy received(&request,&Meteo::JsonRequest::received), failed(&request,&Meteo::JsonRequest::failed);
        request.get(url,60);
        QTRY_COMPARE(received.size(),1);
        QCOMPARE(server.requests.size(),1);
        request.get(url,60);
        request.cancel();
        QTest::qWait(20);
        QCOMPARE(received.size(),1);
        request.get(url,60);
        QTRY_COMPARE(received.size(),2);
        QCOMPARE(server.requests.size(),1);
        request.get(url,60,true);
        QTRY_COMPARE(received.size(),3);
        QCOMPARE(server.requests.size(),2);
        server.status=503;
        request.get(url,60,true);
        QTRY_COMPARE(failed.size(),1);
        request.get(url,60);
        QTRY_COMPARE(received.size(),4); // A failed refresh cannot replace valid data.
        QCOMPARE(server.requests.size(),3);
        const auto hash=QCryptographicHash::hash(url.toEncoded(),QCryptographicHash::Sha256).toHex();
        QFile file(CacheStorage::directory()+"/metadata/"+QString::fromLatin1(hash)+".json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto entry=QJsonDocument::fromJson(file.readAll()).object();file.close();
        entry["savedAt"]=QDateTime::currentDateTimeUtc().addSecs(-120).toString(Qt::ISODate);
        QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));
        file.write(QJsonDocument(entry).toJson());file.close();
        request.get(url,60);
        QTRY_COMPARE(failed.size(),2);
        QCOMPARE(server.requests.size(),4);
        server.status=200;
        request.get(server.endpoint().resolved(QUrl("/cache?model=gfs")),60);
        QTRY_COMPARE(received.size(),5);
        QCOMPARE(server.requests.size(),5);
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
        QCOMPARE(model.tileUrl(35,22,6).host(),server.endpoint().host());
        QCOMPARE(model.tileUrl(35,22,6).port(),server.endpoint().port());
        QCOMPARE(model.legendUrl().port(),server.endpoint().port());
        QVERIFY(tile.queryItemValue("bbox").contains(','));
        model.setRunIndex(1);
        QCOMPARE(model.lastStep(), 120);
        model.setStep(1000);
        QCOMPARE(model.step(), 120);
        WeatherLayersModel reopened(nullptr,server.endpoint());
        reopened.setActive(true);
        QTRY_VERIFY(!reopened.busy());
        QCOMPARE(reopened.runs().size(),2);
        QCOMPARE(server.requests.size(),1);
        reopened.reload();
        QTRY_VERIFY(!reopened.busy());
        QCOMPARE(server.requests.size(),2);
        const auto accepted=model.runs();
        const auto selectedRun=model.runIndex(), selectedStep=model.step();
        server.response=[](const QUrl &) { return QByteArray(R"({"dates":[{"date":"20261025T06:00:00Z","firstStep":120,"lastStep":1}]})"); };
        model.reload();
        QTRY_VERIFY(!model.busy());
        QVERIFY(!model.error().isEmpty());
        QCOMPARE(model.runs(),accepted);
        QCOMPARE(model.runIndex(),selectedRun);
        QCOMPARE(model.step(),selectedStep);
        model.setStep(999);
        QCOMPARE(model.step(),120);
        const auto requests=server.requests.size();
        WeatherLayersModel afterMalformed(nullptr,server.endpoint());
        afterMalformed.setActive(true);
        QTRY_VERIFY(!afterMalformed.busy());
        QCOMPARE(afterMalformed.runs(),accepted);
        QCOMPARE(server.requests.size(),requests); // Rejected data never replaced the valid disk catalog.
    }
    void mapProtocolValidatesRunsWrapsTheWorldAndHonorsNativeSteps()
    {
        using namespace Meteo;
        const auto parsed=MeteoMapSource::parseRuns(QJsonDocument::fromJson(R"({"dates":[
            {"date":"20261025T06:00:00Z","firstStep":60,"lastStep":3},
            {"date":"2026-10-25T00:00:00Z","firstStep":3,"lastStep":384},
            {"date":"20261024T12:00:00Z","firstStep":3,"lastStep":384},
            {"date":"invalid","firstStep":3,"lastStep":384}]})"),"gfs");
        QVERIFY(parsed); QCOMPARE(parsed->size(),2);
        const auto run=parsed->first();
        QCOMPARE(run.stepHours,3);
        QCOMPARE(run.boundedStep(8),6);
        QCOMPARE(run.boundedStep(-100),3);
        QCOMPARE(run.boundedStep(999),384);
        QVERIFY(!run.sourceRevision.isEmpty());
        MeteoMapSource source(QUrl("https://maps.example.test"));
        const auto left=source.tileUrl("test",run,8,-1,0,2);
        QCOMPARE(left,source.tileUrl("test",run,8,3,0,2));
        QCOMPARE(source.tileUrl("test",run,8,4,0,2),source.tileUrl("test",run,8,0,0,2));
        QCOMPARE(left.host(),"maps.example.test");
        QCOMPARE(QUrlQuery(left).queryItemValue("DIM_FORECAST"),"6");
        QVERIFY(source.tileUrl("test",run,8,0,-1,2).isEmpty());
        QVERIFY(source.tileUrl("test",run,8,0,4,2).isEmpty());
        QVERIFY(!MeteoMapSource::parseRuns(QJsonDocument::fromJson("{}"),"gfs"));
        QVERIFY(!MeteoMapSource::parseRuns(QJsonDocument::fromJson(R"({"dates":[]})"),"unknown"));
    }
    void thematicMapsUseUtcPublicationRulesAndOnlyRewindForMissingImages()
    {
        qint64 now=QDateTime(QDate(2026,10,25),QTime(13,5),QTimeZone::UTC).toSecsSinceEpoch();
        ThematicMapsModel model(nullptr,QUrl("https://maps.example.test"),[&] { return now; });
        const auto latest=QDateTime(QDate(2026,10,25),QTime(12,0),QTimeZone::UTC).toSecsSinceEpoch();
        QCOMPARE(model.run(),double(latest));
        QCOMPARE(model.catalog().size(),19);
        QCOMPARE(model.animationUrls().size(),60);
        QVERIFY(model.imageUrl().toEncoded().contains("2026102512"));
        QCOMPARE(model.imageUrl().host(),"maps.example.test");
        model.setLayerIndex(1);
        QVERIFY(model.imageUrl().toEncoded().contains("T%2BWH"));
        model.imageFailed(model.imageUrl(),"connection failed",0);
        QCOMPARE(model.run(),double(latest));
        QCOMPARE(model.attempts(),0);
        QCOMPARE(model.error(),"connection failed");
        model.imageReady(model.imageUrl());
        QVERIFY(model.error().isEmpty());
        model.imageFailed(model.imageUrl(),"unavailable",503);
        QCOMPARE(model.run(),double(latest));
        QCOMPARE(model.error(),"unavailable");
        QSignalSpy retry(&model,&ThematicMapsModel::reloadRequested);
        model.latest();
        QCOMPARE(retry.count(),1);
        QVERIFY(model.error().isEmpty());
        const auto obsolete=model.imageUrl();
        for (int i=0;i<4;++i) model.imageFailed(model.imageUrl(),"missing",404);
        QCOMPARE(model.attempts(),4);
        QCOMPARE(model.run(),double(latest-24*3600));
        model.imageFailed(model.imageUrl(),"missing",404);
        QVERIFY(!model.error().isEmpty());
        const auto previous=model.run();
        model.imageFailed(obsolete,"late response",404);
        QCOMPARE(model.run(),previous);
        model.latest(); model.earlier();
        QVERIFY(model.canGoLater());
        model.later(); QCOMPARE(model.run(),double(latest));
        QVERIFY(!model.canGoLater());
        model.setLayerIndex(16);
        QCOMPARE(model.stepHours(),3);
        QCOMPARE(model.animationUrls().size(),24);
        model.setStep(5); QCOMPARE(model.step(),3);
        QCOMPARE(model.nextStep(),6);
        model.setStep(72); QCOMPARE(model.nextStep(),3);
        now=QDateTime(QDate(2026,10,26),QTime(1,0),QTimeZone::UTC).toSecsSinceEpoch();
        model.latest();
        QVERIFY(model.imageUrl().toEncoded().contains("2026102600"));
    }
    void editorialRetriesTheLastFailedPageAndRejectsMalformedContent()
    {
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [](const QUrl &url) {
            return QUrlQuery(url).queryItemValue("page") == "1"
                ? QByteArray(R"([{"title":"First","content":"first"}])")
                : QByteArray(R"([{"title":"Second","content":"second"}])");
        };
        EditorialModel model(nullptr, server.endpoint());
        model.setCommentary(false);
        model.reload(false);
        QTRY_VERIFY(!model.busy());
        QCOMPARE(model.articles().size(), 1);
        server.status = 503;
        model.more();
        QTRY_VERIFY(!model.busy());
        QCOMPARE(model.articles().size(), 1);
        QVERIFY(!model.error().isEmpty());
        server.status = 200;
        model.more();
        QTRY_VERIFY(!model.busy());
        QCOMPARE(model.articles().size(), 2);
        QStringList pages;
        for (const auto &request : server.requests) pages << QUrlQuery(request).queryItemValue("page");
        QCOMPARE(pages, QStringList({"1", "2", "2"}));
        server.response = [](const QUrl &) { return QByteArray(R"({"error":"unexpected object"})"); };
        model.reload();
        QTRY_VERIFY(!model.busy());
        QVERIFY(!model.error().isEmpty());
        QCOMPARE(model.articles().size(), 1); // Valid cached page one survives the bad response.
        QCOMPARE(model.articles().first().toMap()["title"].toString(), "First");
        EditorialModel reopened(nullptr, server.endpoint());
        reopened.setCommentary(false);
        reopened.reload(false);
        QVERIFY(!reopened.busy());
        QCOMPARE(reopened.articles().first().toMap()["title"].toString(), "First");
        server.response = [](const QUrl &) { return QByteArray(R"([{"title":42,"content":"wrong"}])"); };
        reopened.more();
        QTRY_VERIFY(!reopened.busy());
        QCOMPARE(reopened.articles().size(), 2); // Cached second page is valid and remains available.
        reopened.more();
        QTRY_VERIFY(!reopened.busy());
        QCOMPARE(reopened.articles().size(), 2);
        QVERIFY(!reopened.error().isEmpty());
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
    void editorialReusesDiskCacheAcrossViewsAndKeepsItOnFailure_data()
    {
        QTest::addColumn<bool>("commentary");
        QTest::newRow("articles") << false;
        QTest::newRow("commentary") << true;
    }
    void editorialReusesDiskCacheAcrossViewsAndKeepsItOnFailure()
    {
        QFETCH(bool, commentary);
        ServiceFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.response = [commentary](const QUrl &url) {
            if (commentary) return QByteArray(R"({"content":"<p>Saved comment</p>","published_date":"2026-10-25"})");
            return QUrlQuery(url).queryItemValue("page")=="2"
                ? QByteArray(R"([{"title":"Older article","content":"<p>Older</p>"}])")
                : QByteArray(R"([{"title":"Latest article","content":"<p>Latest</p>"}])");
        };
        const auto endpoint=server.endpoint();
        {
            EditorialModel first(nullptr,endpoint);
            first.setCommentary(commentary);
            first.reload(false);
            QTRY_VERIFY(!first.busy());
            QCOMPARE(server.requests.size(),1);
            if (!commentary) {
                first.more(); QTRY_VERIFY(!first.busy());
                QCOMPARE(first.articles().size(),2);
            }
        }
        const auto requests=server.requests.size();
        EditorialModel reopened(nullptr,endpoint);
        reopened.setCommentary(commentary);
        reopened.reload(false);
        QVERIFY(!reopened.busy());
        QCOMPARE(server.requests.size(),requests);
        if (commentary) QVERIFY(reopened.commentHtml().contains("Saved comment"));
        else {
            QCOMPARE(reopened.articles().size(),1);
            reopened.more();
            QVERIFY(!reopened.busy());
            QCOMPARE(reopened.articles().size(),2);
            QCOMPARE(server.requests.size(),requests);
        }
        reopened.reload(); // Explicit refresh bypasses the fresh cache.
        QTRY_VERIFY(!reopened.busy());
        QCOMPARE(server.requests.size(),requests+1);
        server.close();
        reopened.reload();
        QTRY_VERIFY(!reopened.busy());
        QVERIFY(!reopened.notice().isEmpty());
        QVERIFY(reopened.error().isEmpty());
        if (commentary) QVERIFY(reopened.commentHtml().contains("Saved comment"));
        else QCOMPARE(reopened.articles().size(),1);
    }
    void editorialCalendarNotifiesWhenMidnightChangesTheCurrentDay()
    {
        QDate today(2026,10,25);
        EditorialModel model(nullptr,QUrl("https://example.test"),[&] { return today; });
        QCOMPARE(model.today(),"2026-10-25");
        QSignalSpy changed(&model,&EditorialModel::changed);
        QVERIFY(QMetaObject::invokeMethod(&model,"updateCalendarDay"));
        QCOMPARE(changed.count(),0);
        today=today.addDays(1);
        QVERIFY(QMetaObject::invokeMethod(&model,"updateCalendarDay"));
        QCOMPARE(model.today(),"2026-10-26");
        QCOMPARE(changed.count(),1);
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
    void cacheStorageCountsFilesAndClearsOnlyCache()
    {
        // This test process has a private XDG_CACHE_HOME, never the user's data.
        CacheStorage cache;
        cache.clear();
        QTRY_VERIFY(!cache.busy());
        const auto root=CacheStorage::directory();
        const auto write=[](const QString &path,int bytes) {
            if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(QByteArray(bytes,'x'))==bytes;
        };
        QVERIFY(write(root+"/forecast.json",100));
        QVERIFY(write(root+"/editorial/article.json",200));
        QVERIFY(write(root+"/maps/test/data/tile",300));
        QVERIFY(write(root+"/metadata/search.json",400));
        const auto outside=m_config.filePath("keep-profile.ini");
        QVERIFY(write(outside,400));
#ifndef Q_OS_WIN
        QVERIFY(QFile::link(m_config.path(),root+"/outside-link"));
#endif
        cache.refresh();
        QTRY_VERIFY(!cache.busy());
        QCOMPARE(cache.size(),1000);
        QVERIFY(!cache.sizeText().isEmpty());
        QSignalSpy cleared(&cache,&CacheStorage::cleared);
        cache.clear();
        QTRY_VERIFY(!cache.busy());
        QCOMPARE(cleared.size(),1);
        QCOMPARE(cache.size(),0);
        QVERIFY(cache.error().isEmpty());
        QVERIFY(QFile::exists(outside));
        QCOMPARE(QFileInfo(outside).size(),400);
        QVERIFY(!QFile::exists(root+"/forecast.json"));
        QVERIFY(!QFile::exists(root+"/editorial/article.json"));
        QVERIFY(!QFile::exists(root+"/maps/test/data/tile"));
        QVERIFY(!QFile::exists(root+"/metadata/search.json"));
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
        QVERIFY(!preferences.showLegend());
        preferences.setShowLegend(true);
        UserPreferences legendReload;
        QVERIFY(legendReload.showLegend());
        preferences.setShowLegend(false);
        QCOMPARE(preferences.probeMode(), "chart");
        preferences.setProbeMode("all");
        UserPreferences probeReload;
        QCOMPARE(probeReload.probeMode(), "all");
        probeReload.setProbeMode("invalid");
        QCOMPARE(probeReload.probeMode(), "all");
        probeReload.setProbeMode("off");
        QCOMPARE(preferences.probeMode(), "all");
        probeReload.setProbeMode("chart");
        QCOMPARE(preferences.renderer(), "gpu");
        preferences.setRenderer("cpu");
        UserPreferences rendererReload;
        QCOMPARE(rendererReload.renderer(), "cpu");
        rendererReload.setRenderer("invalid");
        QCOMPARE(rendererReload.renderer(), "cpu");
        rendererReload.setRenderer("gpu");
        QCOMPARE(preferences.visibleGroups(), QStringList({"temperature", "precipitation", "probability", "pressure", "wind", "cloudHeights", "clouds"}));
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
    void savedPlacesNotifyOtherInstancesAfterAtomicSettingsWrites()
    {
        UserPreferences application, widget;
        QSignalSpy changed(&widget, &UserPreferences::placesChanged);
        application.savePlace("Kielce", 50.8661, 20.6286);
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(widget.favorites().size(), 1);
        changed.clear();
        application.savePlace("Warszawa", 52.2297, 21.0122);
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(widget.favorites().size(), 2);
        changed.clear();
        application.removePlace(50.8661, 20.6286);
        QTRY_COMPARE(changed.size(), 1);
        QCOMPARE(widget.favorites().first().toMap()["name"].toString(), "Warszawa");
        QTest::qWait(100);
        QCOMPARE(changed.size(), 1);
    }
    void savedPlacesPreserveOldFavoritesAndRecentsWithoutDuplicates()
    {
        QSettings legacy(QSettings::IniFormat, QSettings::UserScope, "meteo-pl", "desktop");
        const QVariantMap first{{"name", "Kielce"}, {"latitude", 50.8661}, {"longitude", 20.6286}};
        const QVariantMap second{{"name", "Warszawa"}, {"latitude", 52.2297}, {"longitude", 21.0122}};
        legacy.setValue("places/favorites", QJsonDocument::fromVariant(QVariantList{first}).toJson());
        legacy.setValue("places/recent", QJsonDocument::fromVariant(QVariantList{first, second}).toJson());
        legacy.sync();
        UserPreferences preferences;
        QCOMPARE(preferences.favorites().size(), 2);
        preferences.rememberPlace("Gdańsk", 54.352, 18.6466);
        preferences.rememberPlace("Gdańsk", 54.352, 18.6466);
        QCOMPARE(preferences.favorites().size(), 3);
        preferences.savePlace("Kielce centrum", 50.8661, 20.6286);
        QCOMPARE(preferences.favorites().size(), 3);
        QCOMPARE(preferences.favorites().first().toMap()["name"].toString(), "Kielce centrum");
        preferences.removePlace(52.2297, 21.0122);
        UserPreferences reopened;
        QCOMPARE(reopened.favorites().size(), 2);
        QVERIFY(!reopened.isFavorite(52.2297, 21.0122));
    }
    void presetsCanBeEditedDeletedAndRestoredIndependently()
    {
        UserPreferences preferences;
        const auto defaults = preferences.chartLayout();
        const auto basic = preferences.chartLayoutForProfile("basic");
        const auto count = preferences.profiles().size();
        preferences.setChartEnabled("probability", false);
        preferences.setProfileChartHeight(240);
        preferences.renameProfile("My ICM");
        QCOMPARE(preferences.profile(), "classic");
        const auto modified = preferences.chartLayout();
        const auto copy = preferences.duplicateProfile();
        QVERIFY(copy.startsWith("user:"));
        QCOMPARE(preferences.chartLayout(), modified);
        QCOMPARE(preferences.profileChartHeight(), 240);
        preferences.setProfile("classic");
        preferences.resetProfile();
        QCOMPARE(preferences.profile(), "classic");
        QCOMPARE(preferences.chartLayout(), defaults);
        QCOMPARE(preferences.profileChartHeight(), 0);
        QCOMPARE(preferences.profiles().first().toMap()["name"], builtinChartProfiles().first().toMap()["name"]);
        QCOMPARE(preferences.chartLayoutForProfile(copy), modified);
        QCOMPARE(preferences.chartLayoutForProfile("basic"), basic);
        preferences.deleteProfile();
        QVERIFY(preferences.profile() != "classic");
        QVERIFY(preferences.hasDeletedProfiles());
        for (const auto &entry : preferences.profiles()) QVERIFY(entry.toMap()["id"] != "classic");
        UserPreferences reopened;
        QVERIFY(reopened.hasDeletedProfiles());
        reopened.restoreDeletedProfiles();
        reopened.setProfile("classic");
        QCOMPARE(reopened.chartLayout(), defaults);
        QCOMPARE(reopened.chartLayoutForProfile(copy), modified);
        QCOMPARE(reopened.chartHeightForProfile(copy), 240);
        reopened.setProfile(copy);
        reopened.deleteProfile();
        QCOMPARE(reopened.profiles().size(), count);
    }
    void addingTemplatesKeepsChartsLocalToTheChosenProfile()
    {
        UserPreferences preferences;
        preferences.setProfile("basic");
        const auto basic = preferences.chartLayout();
        const auto classic = preferences.chartLayoutForProfile("classic");
        const auto copy = preferences.duplicateProfile();
        preferences.addChart("ground");
        QCOMPARE(preferences.chartLayout().size(), basic.size() + 1);
        QCOMPARE(preferences.chartLayout().last().toMap()["key"].toString(), "ground");
        preferences.setChartOptions("ground", {{"height", 210}});
        preferences.addChart("ground");
        QCOMPARE(preferences.chartLayout().size(), basic.size() + 1);
        QCOMPARE(preferences.chartLayout().last().toMap()["height"].toInt(), 210);
        preferences.addChart("unknown");
        QCOMPARE(preferences.chartLayout().size(), basic.size() + 1);
        preferences.setProfile("basic");
        QCOMPARE(preferences.chartLayout(), basic);
        QCOMPARE(preferences.chartLayoutForProfile("classic"), classic);
        preferences.setProfile(copy);
        QCOMPARE(preferences.chartLayout().last().toMap()["height"].toInt(), 210);
    }
    void retiredPresetsKeepEditedSnapshotsAsUserProfiles()
    {
        const auto current = builtinChartProfiles();
        auto previous = current;
        auto retired = current.first().toMap();
        retired["id"] = "retired-test";
        retired["name"] = "Retired preset";
        retired["revision"] = 4;
        previous.append(retired);
        UserPreferences old(nullptr, previous);
        old.setProfile("retired-test");
        old.renameProfile("My weather");
        old.setProfileChartHeight(250);
        old.setChartOptions("temperature", {{"title", "My temperature"}});
        const auto edited = old.chartLayout();

        UserPreferences upgraded(nullptr, current);
        QCOMPARE(upgraded.profile(), "retired-test");
        QCOMPARE(upgraded.chartLayout(), edited);
        QCOMPARE(upgraded.profileChartHeight(), 250);
        const auto profile = upgraded.profiles().last().toMap();
        QCOMPARE(profile["name"].toString(), "My weather");
        QCOMPARE(profile["revision"].toInt(), 4);
        QVERIFY(!profile["builtin"].toBool());
        QVERIFY(!profile["updateAvailable"].toBool());

        QTemporaryDir directory;
        const auto url = QUrl::fromLocalFile(directory.filePath("retired.json"));
        QVERIFY(upgraded.exportProfilesByIds(url, {"retired-test"})["ok"].toBool());
        const auto exported = ProfileFileCodec::read(url);
        QVERIFY(exported);
        QCOMPARE(exported->first().toMap()["charts"].toList(), edited);
        QCOMPARE(exported->first().toMap()["revision"].toInt(), 4);
        upgraded.deleteProfile();
        QCOMPARE(upgraded.profiles().size(), current.size());
        QCOMPARE(upgraded.profile(), current.first().toMap()["id"].toString());
    }
    void newerPresetRevisionsReplaceUntouchedProfilesAndBackUpEditedOnes()
    {
        auto firstRelease = builtinChartProfiles();
        for (auto &entry : firstRelease) {
            auto preset = entry.toMap();
            preset["revision"] = 7;
            entry = preset;
        }
        UserPreferences old(nullptr, firstRelease);
        old.renameProfile("Edited ICM");
        old.setChartOptions("temperature", {{"title", "My temperature"}});
        old.setProfileChartHeight(240);
        const auto edited = old.chartLayout();
        const auto oldName = Meteo::i18n("%1 (old)").arg("Edited ICM");
        const auto collision = old.createProfile(oldName);
        old.setProfile("classic");
        auto secondRelease = firstRelease;
        QVariantList nextClassic, nextBasic;
        for (auto &value : secondRelease) {
            auto preset = value.toMap();
            preset["revision"] = preset["revision"].toInt() + 1;
            preset["chartHeight"] = 300;
            auto charts = preset["charts"].toList();
            auto chart = charts.first().toMap();
            chart["title"] = "New factory chart";
            auto styles = chart["series"].toList();
            auto style = styles.first().toMap();
            style["color"] = "#214263";
            styles[0] = style;
            chart["series"] = styles;
            charts[0] = chart;
            preset["charts"] = charts;
            if (preset["id"] == "classic") nextClassic = charts;
            if (preset["id"] == "basic") nextBasic = charts;
            value = preset;
        }
        UserPreferences upgraded(nullptr, secondRelease);
        QCOMPARE(upgraded.chartLayout(), edited);
        QCOMPARE(upgraded.profileChartHeight(), 240);
        QCOMPARE(upgraded.chartLayoutForProfile("basic"), nextBasic);
        QCOMPARE(upgraded.chartHeightForProfile("basic"), 300);
        const auto metadata = [&upgraded](const QString &id) {
            for (const auto &entry : upgraded.profiles()) if (entry.toMap()["id"] == id) return entry.toMap();
            return QVariantMap{};
        };
        QVERIFY(metadata("classic")["updateAvailable"].toBool());
        QVERIFY(!metadata("basic")["updateAvailable"].toBool());
        upgraded.setProfile("basic");
        upgraded.updateProfile("classic"); // A widget can update its profile without selecting it globally.
        QCOMPARE(upgraded.profile(), "basic");
        QCOMPARE(upgraded.chartLayoutForProfile("classic"), nextClassic);
        QCOMPARE(upgraded.chartHeightForProfile("classic"), 300);
        QVERIFY(!metadata("classic")["updateAvailable"].toBool());
        const auto backup = upgraded.profiles().last().toMap();
        QCOMPARE(backup["revision"].toInt(), 7);
        QCOMPARE(backup["name"].toString(), oldName + " (2)");
        QCOMPARE(upgraded.chartLayoutForProfile(backup["id"].toString()), edited);
        QCOMPARE(upgraded.chartHeightForProfile(backup["id"].toString()), 240);
        QCOMPARE(upgraded.chartLayoutForProfile(collision), edited);
        const auto count = upgraded.profiles().size();
        upgraded.updateProfile("classic");
        QCOMPARE(upgraded.profiles().size(), count);
    }
    void restoringDeletedPresetsPreservesNamedProfilesAndPreventsAnEmptyList()
    {
        UserPreferences preferences;
        const auto copy = preferences.duplicateProfile();
        const auto charts = preferences.chartLayout();
        for (const auto &entry : builtinChartProfiles()) {
            preferences.setProfile(entry.toMap()["id"].toString());
            preferences.deleteProfile();
        }
        QCOMPARE(preferences.profiles().size(), 1);
        QCOMPARE(preferences.profile(), copy);
        preferences.deleteProfile();
        QCOMPARE(preferences.profiles().size(), 1);
        preferences.restoreDeletedProfiles();
        QCOMPARE(preferences.profiles().size(), builtinChartProfiles().size() + 1);
        QCOMPARE(preferences.profile(), copy);
        QCOMPARE(preferences.chartLayout(), charts);
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
        const auto exportedCount = preferences.profiles().size();
        QCOMPARE(preferences.exportProfiles(all, true)["count"].toInt(), exportedCount);
        QCOMPARE(preferences.importProfiles(all)["count"].toInt(), exportedCount);
        QCOMPARE(preferences.profiles().last().toMap()["name"].toString(), QStringLiteral("Góry zimą (2) (2)"));
        UserPreferences reopened;
        QCOMPARE(reopened.profiles().size(), builtinChartProfiles().size() + 2 + exportedCount);
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
    void savedWindChartsCombineOnceAndPreserveArrowStyle()
    {
        QTemporaryDir config; QVERIFY(config.isValid());
        qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
        auto arrow = defaultSeriesStyle("wind10_dr_deg_true_prev_point");
        arrow["color"] = "#123456"; arrow["lineWidth"] = 3.0;
        const QVariantList old{
            QVariantMap{{"key","wind"},{"title","Mój wiatr"},{"enabled",true},
                {"series",QVariantList{defaultSeriesStyle("wind10_sd_true_prev_point"),defaultSeriesStyle("wind_gust_max")}}},
            QVariantMap{{"key","windDirection"},{"enabled",true},{"series",QVariantList{arrow}}}};
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,"meteo-pl","desktop");
        settings.setValue("display/layoutVersion",2);
        settings.setValue("display/profile","user:test");
        settings.setValue("display/customLayout",QJsonDocument::fromVariant(old).toJson());
        settings.setValue("display/savedProfiles",QJsonDocument::fromVariant(QVariantList{
            QVariantMap{{"id","user:test"},{"name","Wiatr"},{"charts",old}}}).toJson());
        settings.sync();
        UserPreferences preferences;
        QCOMPARE(preferences.profile(),"user:test");
        QCOMPARE(preferences.visibleGroups(),QStringList{"wind"});
        const auto chart=preferences.chartLayout().first().toMap();
        QCOMPARE(chart["title"].toString(),"Mój wiatr");
        const auto series=chart["series"].toList(); QCOMPARE(series.size(),3);
        QCOMPARE(series.last().toMap()["color"].toString(),"#123456");
        QCOMPARE(series.last().toMap()["lineWidth"].toDouble(),3.0);
        preferences.setProfile("custom");
        QCOMPARE(preferences.visibleGroups(),QStringList{"wind"});
        preferences.setChartEnabled("windDirection",true);
        UserPreferences reopened;
        QVERIFY(reopened.visibleGroups().contains("windDirection")); // A later explicit choice stays intact.
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
        QCOMPARE(preferences.recentPlaces().first().toMap()["name"].toString(), "Chęciny");
        QCOMPARE(preferences.placeAt(50.8, 20.4)["name"].toString(), "Chęciny");
        preferences.toggleFavorite("Chęciny", 50.8, 20.4);
        QVERIFY(!preferences.isFavorite(50.8, 20.4));
    }
};
QTEST_GUILESS_MAIN(ServicesTest)
#include "test_services.moc"
