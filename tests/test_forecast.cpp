#include "core/Forecast.hpp"
#include "Fixture.hpp"
#include <QTest>

class ForecastTest : public QObject {
    Q_OBJECT
private slots:
    void parsesSeriesWithoutChangingUnits()
    {
        const auto result = Meteo::parseForecast(forecastFixture(1700000000));
        QVERIFY(result);
        QCOMPARE(result->firstTimestamp(), 1700000000);
        QCOMPARE(result->lastTimestamp(), 1700010800);
        QCOMPARE(result->series["airtmp_point"].values[0].value(), 10.0);
        QCOMPARE(result->series["slpres_point"].values[0].value(), 101200.0);
        QVERIFY(!result->series["airtmp_point"].at(1699999999));
        QVERIFY(!result->series["airtmp_point"].at(1700014400));
    }
    void preservesMissingNumbers()
    {
        auto input = forecastFixture();
        input.replace("[10,12,9,11]", "[10,nan,null,11]");
        const auto result = Meteo::parseForecast(input);
        QVERIFY(result);
        QVERIFY(!result->series["airtmp_point"].values[1]);
        QVERIFY(!result->series["airtmp_point"].values[2]);
        auto object = QJsonDocument::fromJson(forecastFixture()).object();
        auto data = object["data"].toObject();
        auto cloud = data["cldbse25"].toObject();
        cloud["data"] = QJsonArray{-327276480.0, 1000.0};
        data["cldbse25"] = cloud;
        object["data"] = data;
        const auto heights = Meteo::parseForecast(QJsonDocument(object).toJson());
        QVERIFY(heights);
        QVERIFY(!heights->series["cldbse25"].values[0]);
        QCOMPARE(heights->series["cldbse25"].values[1].value(), 1000.0);
    }
    void rejectsBrokenDocuments()
    {
        for (const QByteArray input : {QByteArray("{}"), QByteArray("<html>error</html>"), QByteArray("{\"data\":[]}")})
            QVERIFY(!Meteo::parseForecast(input));
        auto object = QJsonDocument::fromJson(forecastFixture()).object();
        auto data = object["data"].toObject();
        auto field = data["airtmp_point"].toObject();
        field["interval"] = 0;
        data["airtmp_point"] = field;
        object["data"] = data;
        QVERIFY(!Meteo::parseForecast(QJsonDocument(object).toJson()));
    }
    void findsNewestRunInUnsortedList()
    {
        const auto run = Meteo::latestRun(R"({"um4_60":[100,300,200]})", "um4_60");
        QVERIFY(run);
        QCOMPARE(*run, 300);
        QVERIFY(!Meteo::latestRun("{}", "um4_60"));
        QVERIFY(!Meteo::latestRun(R"({"other":[300]})", "other"));
    }
    void validatesModelArea()
    {
        QVERIFY(Meteo::validLocation(50.8661, 20.6286, "um4_60"));
        QVERIFY(!Meteo::validLocation(40.7, -74.0, "um4_60"));
        QVERIFY(Meteo::validLocation(40.7, -74.0, "gfs"));
        QVERIFY(!Meteo::validLocation(91, 20, "gfs"));
        QVERIFY(!Meteo::validLocation(50, 20, "invalid"));
    }
};
QTEST_GUILESS_MAIN(ForecastTest)
#include "test_forecast.moc"
