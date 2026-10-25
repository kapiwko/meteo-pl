#include "core/Forecast.hpp"
#include "core/ForecastAggregation.hpp"
#include "core/ForecastSampling.hpp"
#include "Fixture.hpp"
#include <QTest>

class ForecastTest : public QObject {
    Q_OBJECT
private slots:
    void samplesNativeAxesAccordingToQuantityTiming()
    {
        using namespace Meteo;
        const Series series{"unit", 1000, 10800, {1,9,std::nullopt,4}};
        for (auto timing : {QuantityTiming::Instant, QuantityTiming::IntervalMean, QuantityTiming::IntervalTotal,
                            QuantityTiming::IntervalExtremum, QuantityTiming::Category}) {
            QVERIFY(!sampleSeries(series,999,timing));
            QCOMPARE(sampleSeries(series,1000,timing)->value.value(),1.0);
            const auto selected = sampleSeries(series,8200,timing);
            QVERIFY(selected);
            QCOMPARE(selected->sourceTimestamp, timing == QuantityTiming::Instant ? 11800 : 1000);
            QCOMPARE(selected->value.value(), timing == QuantityTiming::Instant ? 9.0 : 1.0);
            QCOMPARE(selected->intervalTo,11800);
            QCOMPARE(selected->timing,timing);
            QCOMPARE(sampleSeries(series,11800,timing)->value.value(),9.0);
            QVERIFY(!sampleSeries(series,22600,timing)->value);
            QCOMPARE(sampleSeries(series,33400,timing)->value.value(),4.0);
            QVERIFY(!sampleSeries(series,44200,timing));
            QCOMPARE(bool(sampleSeries(series,33401,timing)), timing != QuantityTiming::Instant);
        }
        QCOMPARE(sampleSeries(series,6400,QuantityTiming::Instant)->sourceTimestamp,11800); // Tie goes forward.
        QCOMPARE(sampleSeries(series,6399,QuantityTiming::Instant)->sourceTimestamp,1000);
        QCOMPARE(quantityTiming("airtmp_point"),QuantityTiming::Instant);
        QCOMPARE(quantityTiming("pcpttl_aver"),QuantityTiming::IntervalTotal);
        QCOMPARE(quantityTiming("pcpttl_max"),QuantityTiming::IntervalExtremum);
        QCOMPARE(quantityTiming("cldtot_aver"),QuantityTiming::IntervalMean);
        QCOMPARE(quantityTiming("pcpttl_type_max"),QuantityTiming::Category);
    }
    void sumsPrecipitationOnItsNativeTimeline()
    {
        const auto first = QDateTime(QDate(2026,10,25),QTime(0,0),QTimeZone::utc()).toSecsSinceEpoch();
        Meteo::Forecast forecast;
        forecast.series["airtmp_point"] = {"Celsius",first,3600,{10,10,10,10,10,10,10}};
        forecast.series["pcpttl_aver"] = {"kg/m2",first,10800,{1,2,0}};
        const auto days = Meteo::aggregateDays(forecast,QTimeZone::utc());
        QCOMPARE(days.size(),1);
        QCOMPARE(days.first().rain.value(),3.0); // The hourly display used to sum 1+1+1+2+2+2+0 = 9.
        QCOMPARE(days.first().rainCoverageSeconds,9*3600);
        QVERIFY(!days.first().rainComplete);
        forecast.series["pcpttl_aver"].firstTimestamp += 1800;
        QCOMPARE(Meteo::aggregateDays(forecast,QTimeZone::utc()).first().rain.value(),3.0);
    }
    void partitionsNativeIntervalsAtCalendarMidnight()
    {
        const auto first = QDateTime(QDate(2026,10,25),QTime(23,0),QTimeZone::utc()).toSecsSinceEpoch();
        Meteo::Forecast forecast;
        forecast.series["pcpttl_aver"] = {"kg/m2",first,10800,{3}};
        const auto days = Meteo::aggregateDays(forecast,QTimeZone::utc());
        QCOMPARE(days.size(),2);
        QCOMPARE(days[0].rain.value(),1.0); QCOMPARE(days[1].rain.value(),2.0);
        QVERIFY(days[0].rainEstimatedAtBoundary); QVERIFY(days[1].rainEstimatedAtBoundary);
        QCOMPARE(days[0].rainCoverageSeconds,3600); QCOMPARE(days[1].rainCoverageSeconds,7200);
    }
    void preservesMissingPrecipitation()
    {
        const auto first = QDateTime(QDate(2026,10,25),QTime(0,0),QTimeZone::utc()).toSecsSinceEpoch();
        Meteo::Forecast forecast;
        forecast.series["pcpttl_aver"] = {"kg/m2",first,10800,{std::nullopt,2}};
        auto day = Meteo::aggregateDays(forecast,QTimeZone::utc()).first();
        QCOMPARE(day.rain.value(),2.0); QVERIFY(!day.rainComplete);
        forecast.series["pcpttl_aver"].values = {std::nullopt,std::nullopt};
        day = Meteo::aggregateDays(forecast,QTimeZone::utc()).first();
        QVERIFY(!day.rain); QCOMPARE(day.rainCoverageSeconds,0);
    }
    void usesElapsedTimeAcrossDaylightSavingChanges()
    {
        const QTimeZone zone("Europe/Warsaw");
        QVERIFY(zone.isValid());
        Meteo::Forecast forecast;
        const auto spring = QDateTime(QDate(2026,3,29),QTime(0,0),zone).toSecsSinceEpoch();
        forecast.series["pcpttl_aver"] = {"kg/m2",spring,86400,{24}};
        auto days = Meteo::aggregateDays(forecast,zone);
        QCOMPARE(days.size(),2); QCOMPARE(days[0].rain.value(),23.0); QCOMPARE(days[1].rain.value(),1.0);
        QVERIFY(days[0].rainComplete); QVERIFY(!days[1].rainComplete);
        const auto autumn = QDateTime(QDate(2026,10,25),QTime(0,0),zone).toSecsSinceEpoch();
        forecast.series["pcpttl_aver"] = {"kg/m2",autumn,86400,{24,24}};
        days = Meteo::aggregateDays(forecast,zone);
        QCOMPARE(days.size(),2); QCOMPARE(days[0].rain.value(),25.0); QCOMPARE(days[1].rain.value(),23.0);
        QVERIFY(days[0].rainComplete);
    }
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
    void acceptsUnavailableOptionalSeries()
    {
        auto object = QJsonDocument::fromJson(forecastFixture()).object();
        auto data = object["data"].toObject();
        data["airtmp_min"] = QJsonValue::Null;
        data["airtmp_max"] = QJsonValue::Null;
        data["cldbse25"] = QJsonValue::Null;
        data["storm_max"] = QJsonValue::Null;
        object["data"] = data;
        const auto result = Meteo::parseForecast(QJsonDocument(object).toJson());
        QVERIFY(result);
        QVERIFY(!result->series.contains("airtmp_min"));
        QVERIFY(!result->series.contains("cldbse25"));
        QVERIFY(result->series.contains("pcpttl_aver"));
        data["airtmp_point"] = QJsonValue::Null;
        object["data"] = data;
        QVERIFY(!Meteo::parseForecast(QJsonDocument(object).toJson()));
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
