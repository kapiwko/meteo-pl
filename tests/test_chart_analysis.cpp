#include "ChartAnalysis.hpp"
#include "ChartConfiguration.hpp"
#include "ChartDataBuilder.hpp"
#include <QTest>
#include <QFile>
#include <QJSEngine>
#include <QSet>
#include <cmath>
#include <limits>

namespace {
QJSValue recordChart(QJSEngine &engine, const QVariantList &fields)
{
    QFile source(":/qt/qml/pl/meteo/desktop/MeteogramStyle.js");
    if (!source.open(QIODevice::ReadOnly)) return engine.newErrorObject(QJSValue::GenericError, source.errorString());
    auto script = QString::fromUtf8(source.readAll());
    script.remove(0, script.indexOf('\n') + 1);
    const auto loaded = engine.evaluate(script);
    if (loaded.isError()) return loaded;
    engine.globalObject().setProperty("fieldsUnderTest", engine.toScriptValue(prepareChartFields(fields)));
    return engine.evaluate(QStringLiteral(R"JS(
        (function() {
            const rectangles=[], polygons=[], circles=[], lines=[], ctx={}; let path=[];
            for (const name of ["reset","save","restore","rect","clip","setLineDash","stroke","fillText","translate","rotate"])
                ctx[name]=function(){};
            ctx.beginPath=function(){path=[];};
            ctx.moveTo=ctx.lineTo=function(x,y){path.push([x,y]);};
            ctx.closePath=function(){path.closed=true;};
            ctx.stroke=function(){if(this.strokeStyle==="#123456" && path.length) lines.push(path.slice());};
            ctx.fill=function(){if(path.closed && this.fillStyle==="#123456") polygons.push(path.slice());};
            ctx.fillRect=function(x,y,w,h){if(this.fillStyle==="#123456") rectangles.push({x:x,y:y,width:w,height:h});};
            ctx.arc=function(x,y,r){circles.push({x:x,y:y,radius:r});};
            const root={
                leftMargin:0,rightMargin:0,topMargin:0,bottomMargin:0,directionBandHeight:0,
                darkPalette:false,scaleKind:"linear",directionOnly:false,hasRightAxis:false,
                startTime:1800000000,endTime:1800021600,weather:{firstTimestamp:1800000000},nights:[],
                fields:fieldsUnderTest,bounds:{low:0,high:10},rightBounds:{low:0,high:10},
                ticks:function(){return [];},axisUnit:function(){return "";},
                timeAxisLabels:function(){return {labels:[],days:[],midnights:[]};},
                valid:function(v){return v!==null&&v!==undefined&&isFinite(v);},shown:function(){return true;},
                layerOrder:function(){return 0;},seriesColor:function(){return "#123456";},isRightField:function(){return false;},drawGrid:function(){},
                px:function(t){return (t-1800000000)/3600*100;},py:function(v){return 100-v;},
                pointAt:function(points,time){return points.find(point=>point.time===time);}
            };
            paintChart(root,{width:600,height:120},ctx,false);
            return {rectangles:rectangles,polygons:polygons,circles:circles,lines:lines};
        })()
    )JS"));
}
}

class ChartAnalysisTest : public QObject {
    Q_OBJECT
private slots:
    void autoAxesRejectAThirdUnitButPreserveExplicitChoices()
    {
        const QVariantList points{QVariantMap{{"time", 1800000000LL}, {"value", 2.0}, {"interval", 3600}}};
        QVariantMap series{{"airtmp_point", points}, {"slpres_point", points}, {"wind10_sd_true_prev_point", points},
                            {"wind10_dr_deg_true_prev_point", points}, {"storm_max", points}};
        const auto chart = [&](const QStringList &keys, bool explicitAxes = false, bool fixedScale = false) {
            QVariantList styles;
            for (const auto &key : keys) {
                auto style = defaultSeriesStyle(key);
                if (explicitAxes) style["axis"] = key == "airtmp_point" ? "left" : "right";
                styles.append(style);
            }
            QVariantMap layout{{"key", "chart:mixed"}, {"title", "Mixed"}, {"enabled", true}, {"series", styles},
                               {"leftScale", "auto"}, {"rightScale", fixedScale ? "wind" : "auto"}};
            if (fixedScale) { layout["rightMin"] = 0.0; layout["rightMax"] = 1200.0; }
            return buildConfiguredCharts({layout}, series, "m/s").first().toMap();
        };
        QVERIFY(chart({"airtmp_point"})["axisConflict"].toString().isEmpty());
        QVERIFY(chart({"airtmp_point", "wind10_sd_true_prev_point"})["axisConflict"].toString().isEmpty());
        const auto first = chart({"airtmp_point", "wind10_sd_true_prev_point", "slpres_point"});
        const auto second = chart({"airtmp_point", "slpres_point", "wind10_sd_true_prev_point"});
        QVERIFY(!first["axisConflict"].toString().isEmpty());
        QCOMPARE(first["axisConflict"], second["axisConflict"]);
        QCOMPARE(first["fields"].toList().size(), 3);
        const auto deliberate = chart({"airtmp_point", "wind10_sd_true_prev_point", "slpres_point"}, true);
        QVERIFY(deliberate["axisConflict"].toString().isEmpty());
        QCOMPARE(deliberate["rightScale"].toString(), "linear");
        const auto fixed = chart({"airtmp_point", "wind10_sd_true_prev_point", "slpres_point"}, true, true);
        QCOMPARE(fixed["rightScale"].toString(), "wind");
        QCOMPARE(fixed["rightMax"].toDouble(), 1200.0);
        const auto categorical = chart({"wind10_dr_deg_true_prev_point", "storm_max", "airtmp_point", "wind10_sd_true_prev_point"});
        QVERIFY(categorical["axisConflict"].toString().isEmpty());
        QCOMPARE(categorical["leftUnit"].toString(), "°C");
        QCOMPARE(categorical["rightUnit"].toString(), "m/s");
        series.remove("slpres_point");
        QVERIFY(chart({"airtmp_point", "wind10_sd_true_prev_point", "slpres_point"})["axisConflict"].toString().isEmpty());
        // Explicit right axes with the same unit do not steal the primary auto axis.
        auto humidity = defaultSeriesStyle("realhum_aver"), fog = defaultSeriesStyle("fog_max");
        fog["axis"] = "right";
        const auto sameUnit = buildConfiguredCharts({QVariantMap{{"key", "humidity"}, {"enabled", true},
            {"series", QVariantList{humidity, fog}}}}, {{"realhum_aver", points}, {"fog_max", points}}, "m/s").first().toMap();
        QCOMPARE(sameUnit["fields"].toList().first().toMap()["axis"].toString(), "left");
    }
    void stormEventsCombineBothNativeAxes_data()
    {
        QTest::addColumn<QVariantList>("fields");
        QTest::addColumn<QVariantList>("centers");
        QTest::addColumn<int>("flashes");
        const auto field = [](const QString &kind, int first, int interval, const QVariantList &values) {
            QVariantList points;
            for (qsizetype i = 0; i < values.size(); ++i)
                points.append(QVariantMap{{"time", 1800000000LL + (first + i * interval) * 3600},
                                           {"value", values[i]}, {"interval", interval * 3600}});
            return QVariantMap{{"key", kind == "storm" ? "storm_max" : "flash_max"}, {"kind", kind},
                               {"eventColors", false}, {"points", points}};
        };
        QTest::newRow("storm before flash coverage") << QVariantList{field("storm",0,1,{1.0}),field("flash",1,1,{0.0})} << QVariantList{50.0} << 0;
        QTest::newRow("flash before storm coverage") << QVariantList{field("storm",1,1,{0.0}),field("flash",0,1,{15.0})} << QVariantList{50.0} << 1;
        QTest::newRow("same interval one symbol") << QVariantList{field("storm",0,1,{1.0}),field("flash",0,1,{15.0})} << QVariantList{50.0} << 1;
        QTest::newRow("different overlapping intervals") << QVariantList{field("storm",0,3,{1.0}),field("flash",1,1,{15.0})} << QVariantList{50.0,150.0,250.0} << 1;
        QTest::newRow("zero flash does not split storm") << QVariantList{field("storm",0,3,{1.0}),field("flash",1,1,{0.0})} << QVariantList{150.0} << 0;
        QTest::newRow("only storm enabled") << QVariantList{field("storm",0,3,{1.0})} << QVariantList{150.0} << 0;
        QTest::newRow("only flash enabled with gap") << QVariantList{field("flash",0,3,{QVariant(),15.0})} << QVariantList{450.0} << 1;
        QTest::newRow("nonoverlapping axes") << QVariantList{field("storm",0,1,{1.0}),field("flash",3,1,{15.0})} << QVariantList{50.0,350.0} << 1;
        QTest::newRow("missing data stays missing") << QVariantList{field("storm",0,1,{QVariant()}),field("flash",0,1,{QVariant()})} << QVariantList{} << 0;
    }
    void stormEventsCombineBothNativeAxes()
    {
        QFETCH(QVariantList, fields); QFETCH(QVariantList, centers); QFETCH(int, flashes);
        const auto prepared = prepareChartFields(fields);
        const auto events = prepared.first().toMap().value("events").toList();
        QCOMPARE(events.size(), centers.size());
        QJSEngine engine;
        const auto drawing = recordChart(engine, fields);
        QVERIFY2(!drawing.isError(), qPrintable(drawing.toString()));
        const auto circles = drawing.property("circles");
        QCOMPARE(circles.property("length").toInt(), centers.size() + flashes);
        QSet<double> actual;
        for (int i = 0; i < circles.property("length").toInt(); ++i) actual.insert(circles.property(i).property("x").toNumber());
        QCOMPARE(actual.size(), centers.size());
        for (const auto &center : centers) QVERIFY(actual.contains(center.toDouble()));
    }
    void rangesJoinNativeTimestampsAndPreserveGaps_data()
    {
        QTest::addColumn<QVariantList>("lower");
        QTest::addColumn<QVariantList>("upper");
        QTest::addColumn<int>("segments");
        QTest::addColumn<int>("pairs");
        const auto points = [](const QList<int> &hours, int missing = -1) {
            QVariantList result;
            for (int hour : hours) result.append(QVariantMap{{"time", 1800000000LL + hour * 3600},
                {"value", hour == missing ? QVariant() : QVariant(hour + 1.0)}});
            return result;
        };
        QTest::newRow("short upper") << points({0,1,2,3}) << points({2,3}) << 1 << 2;
        QTest::newRow("short lower") << points({2,3}) << points({0,1,2,3}) << 1 << 2;
        QTest::newRow("shifted equal lengths") << points({0,1,2,3}) << points({2,3,4,5}) << 1 << 2;
        QTest::newRow("no common timestamps") << points({0,1}) << points({2,3}) << 0 << 0;
        QTest::newRow("missing lower") << points({0,1,2,3},1) << points({0,1,2,3}) << 1 << 3;
        QTest::newRow("missing upper") << points({0,1,2,3}) << points({0,1,2,3},1) << 1 << 3;
        QTest::newRow("different intervals") << points({0,2,4}) << points({0,1,2,3,4}) << 2 << 3;
        QTest::newRow("gap inside segment") << points({0,2}) << points({0,1,2},1) << 0 << 2;
    }
    void rangesJoinNativeTimestampsAndPreserveGaps()
    {
        QFETCH(QVariantList, lower); QFETCH(QVariantList, upper);
        QFETCH(int, segments); QFETCH(int, pairs);
        QVariantMap range{{"key", "airtmp_max"}, {"kind", "range"}, {"points", upper}, {"lowerPoints", lower}};
        QJSEngine engine;
        const auto drawing = recordChart(engine, {range});
        QVERIFY2(!drawing.isError(), qPrintable(drawing.toString()));
        QCOMPARE(drawing.property("polygons").property("length").toInt(), segments);
        if (QByteArray(QTest::currentDataTag()) == "short upper") {
            const auto polygon = drawing.property("polygons").property(0);
            QCOMPARE(polygon.property(0).property(0).toNumber(), 200.0);
            QCOMPARE(polygon.property(1).property(0).toNumber(), 300.0);
        }
        range["kind"] = "rangeStems";
        QJSEngine stemsEngine;
        const auto stems = recordChart(stemsEngine, {range});
        QVERIFY2(!stems.isError(), qPrintable(stems.toString()));
        const auto lines = stems.property("lines");
        QCOMPARE(lines.property("length").toInt(), pairs ? 1 : 0);
        if (pairs) QCOMPARE(lines.property(0).property("length").toInt(), pairs * 2);
    }
    void singlePrecipitationSampleUsesItsNativeInterval_data()
    {
        QTest::addColumn<int>("hours");
        for (int hours : {1, 3, 6}) QTest::newRow(qPrintable(QString::number(hours))) << hours;
    }
    void singlePrecipitationSampleUsesItsNativeInterval()
    {
        QFETCH(int, hours);
        Meteo::Forecast forecast;
        forecast.series.insert("pcpttl_aver", {"mm", 1800000000, hours * 3600, {4.0}});
        const auto points = buildChartSeries(forecast, {}).value("pcpttl_aver").toList();
        QCOMPARE(points.first().toMap()["intervalFrom"].toLongLong(), 1800000000LL);
        QCOMPARE(points.first().toMap()["intervalTo"].toLongLong(), 1800000000LL + hours * 3600);
        QJSEngine engine;
        const auto drawing = recordChart(engine, {QVariantMap{{"key", "pcpttl_aver"}, {"kind", "bars"}, {"points", points}}});
        QVERIFY2(!drawing.isError(), qPrintable(drawing.toString()));
        const auto rectangles = drawing.property("rectangles");
        QCOMPARE(rectangles.property("length").toInt(), 1);
        QCOMPARE(rectangles.property(0).property("x").toNumber(), 0.0);
        QCOMPARE(rectangles.property(0).property("width").toNumber(), hours * 100.0);
    }
    void precipitationCaptionIgnoresUnrelatedSeriesAndReportsDifferentPeriods()
    {
        ChartAnalysis analysis;
        const QVariantMap temperature{{"key", "airtmp_point"}, {"points", QVariantList{QVariantMap{{"interval", 3600}}}}};
        const QVariantMap mean{{"key", "pcpttl_aver"}, {"points", QVariantList{QVariantMap{{"interval", 10800}}}}};
        const QVariantMap maximum{{"key", "pcpttl_max"}, {"points", QVariantList{QVariantMap{{"interval", 3600}}}}};
        QCOMPARE(analysis.precipitationCaption({temperature, mean}), QString("Precipitation totals over 3 h."));
        QCOMPARE(analysis.precipitationCaption({mean, temperature}), analysis.precipitationCaption({temperature, mean}));
        QCOMPARE(analysis.precipitationCaption({temperature, maximum, mean}),
                 QString("Mean: total over 3 h. Maximum: total over 1 h."));
        QCOMPARE(analysis.precipitationCaption({mean, maximum}), analysis.precipitationCaption({maximum, mean}));
    }
    void cursorUsesPreparedSampleValuesAndTheirNativePositions()
    {
        QFile source(":/qt/qml/pl/meteo/desktop/MeteogramStyle.js");
        QVERIFY(source.open(QIODevice::ReadOnly));
        auto script = QString::fromUtf8(source.readAll());
        script.remove(0,script.indexOf('\n')+1);
        QJSEngine engine;
        QVERIFY(!engine.evaluate(script).isError());
        const auto result = engine.evaluate(QStringLiteral(R"JS(
            (function() {
                const circles=[], ctx={};
                for (const name of ["save","restore","beginPath","rect","clip","moveTo","lineTo","stroke","fill"])
                    ctx[name]=function(){};
                ctx.arc=function(x,y){circles.push({x:x,y:y});};
                const root={
                    leftMargin:0,rightMargin:0,topMargin:0,bottomMargin:0,directionBandHeight:0,
                    nowTime:-1,startTime:0,endTime:20000,selectedTime:7200,exportMode:false,
                    fields:[{key:"dwptmp_point",kind:"points"},{key:"pcpttl_aver",kind:"bars"}],
                    probeSamples:{dwptmp_point:{number:9,sourceTimestamp:10800},
                                  pcpttl_aver:{number:1,sourceTimestamp:0,intervalFrom:0,intervalTo:10800}},
                    px:function(t){return t/100;},py:function(v){return 100-v;},
                    valid:function(v){return v!==undefined&&v!==null&&isFinite(v);},
                    shown:function(){return true;},seriesColor:function(){return "green";},isRightField:function(){return false;}
                };
                paintCursor(root,{width:200,height:100},ctx,false);
                return circles;
            })()
        )JS"));
        QVERIFY2(!result.isError(),qPrintable(result.toString()));
        QCOMPARE(result.property("length").toInt(),2);
        QCOMPARE(result.property(0).property("x").toNumber(),108.0);
        QCOMPARE(result.property(0).property("y").toNumber(),91.0);
        QCOMPARE(result.property(1).property("x").toNumber(),54.0);
        QCOMPARE(result.property(1).property("y").toNumber(),99.0);
    }
    void signedBarsHaveTheirExpectedGeometry()
    {
        QFile source(":/qt/qml/pl/meteo/desktop/MeteogramStyle.js");
        QVERIFY(source.open(QIODevice::ReadOnly));
        QJSEngine engine;
        auto script = QString::fromUtf8(source.readAll());
        script.remove(0, script.indexOf('\n') + 1); // QML's .pragma is not JavaScript.
        const auto loaded = engine.evaluate(script);
        QVERIFY2(!loaded.isError(), qPrintable(loaded.toString()));
        for (bool right : {false, true}) for (double shift : {-100.0, 0.0, 100.0}) for (double value : {-5.0, 0.0, 5.0}) {
            engine.globalObject().setProperty("testRight", right);
            engine.globalObject().setProperty("testShift", shift);
            engine.globalObject().setProperty("testValue", value);
            const auto geometry = engine.evaluate(QStringLiteral(R"JS(
                (function() {
                    const rectangles = [], ctx = {};
                    for (const name of ["reset", "save", "restore", "beginPath", "rect", "clip", "setLineDash", "stroke", "fillText"])
                        ctx[name] = function() {};
                    ctx.fillRect = function(x,y,width,height) {
                        if (this.fillStyle === "#123456") rectangles.push({x:x,y:y,width:width,height:height});
                    };
                    const root = {
                        leftMargin:0, rightMargin:0, topMargin:0, bottomMargin:0, directionBandHeight:0,
                        darkPalette:false, scaleKind:"linear", directionOnly:false, hasRightAxis:false,
                        startTime:0, endTime:3600, weather:{firstTimestamp:0}, nights:[],
                        fields:[{key:"airtmp_point",kind:"bars",color:"#123456",axis:testRight?"right":"left",
                                 points:[{time:0,value:testValue},{time:3600,value:null}]}],
                        ticks:function(){return [];}, axisUnit:function(){return "";},
                        timeAxisLabels:function(){return {labels:[],days:[],midnights:[]};},
                        valid:function(v){return v!==null && isFinite(v);}, shown:function(){return true;},
                        layerOrder:function(){return 0;}, seriesColor:function(f){return f.color;},
                        isRightField:function(f){return f.axis==="right";}, drawGrid:function(){},
                        px:function(t){return t/3600*50;}, py:function(v,right){return 50+testShift-v*(right?4:2);}
                    };
                    paintChart(root,{width:120,height:100},ctx,false);
                    return rectangles;
                })()
            )JS"));
            QVERIFY2(!geometry.isError(), qPrintable(geometry.toString()));
            QCOMPARE(geometry.property("length").toInt(), 1);
            const auto rectangle = geometry.property(0);
            const double zero = 50 + shift, y = zero - value * (right ? 4 : 2);
            QCOMPARE(rectangle.property("y").toNumber(), std::min(y, zero));
            QCOMPARE(rectangle.property("height").toNumber(), std::abs(zero-y));
        }
    }
    void preservesOrdinaryTicks()
    {
        const auto axis = ChartMath::niceAxis(-10, 10);
        QCOMPARE(axis.low, -10); QCOMPARE(axis.high, 10); QCOMPARE(axis.step, 5);
        QCOMPARE(axis.ticks, QVector<double>({-10, -5, 0, 5, 10}));
        QCOMPARE(ChartMath::axis(0, 100, 20).ticks, QVector<double>({0,20,40,60,80,100}));
    }
    void boundsEveryAcceptedOrDegenerateRange()
    {
        const double maximum = std::numeric_limits<double>::max();
        const double tiny = std::numeric_limits<double>::denorm_min();
        const QVector<QPair<double, double>> ranges{
            {-10,10}, {1e20,1e20+20000}, {1e-300,2e-300}, {tiny,3*tiny},
            {0,0}, {20,-20}, {-maximum,maximum}, {maximum,maximum},
            {std::numeric_limits<double>::infinity(),10}, {0,std::nan("")}};
        for (const auto &[low, high] : ranges) {
            const auto axis = ChartMath::niceAxis(low, high);
            QVERIFY(ChartMath::validRange(axis.low, axis.high));
            QVERIFY(std::isfinite(axis.step)); QVERIFY(axis.step > 0);
            QVERIFY(!axis.ticks.isEmpty()); QVERIFY(axis.ticks.size() <= 64);
            for (qsizetype i=0; i<axis.ticks.size(); ++i) {
                QVERIFY(std::isfinite(axis.ticks[i]));
                if (i) QVERIFY(axis.ticks[i] > axis.ticks[i-1]);
            }
        }
        QCOMPARE(ChartMath::niceAxis(1e20,1e20+20000).ticks.size(), 2);
        for (double step : {0.0, -1.0, tiny, std::numeric_limits<double>::infinity()})
            QVERIFY(ChartMath::axis(0,1,step).ticks.size() <= 64);
    }
    void rejectsOverflowingImportedBounds()
    {
        auto chart = defaultChartLayout("classic").first().toMap();
        chart["leftMin"] = -std::numeric_limits<double>::max();
        chart["leftMax"] = std::numeric_limits<double>::max();
        const auto normalized = normalizeChartLayout({chart}).first().toMap();
        QVERIFY(!normalized.contains("leftMin")); QVERIFY(!normalized.contains("leftMax"));
        chart["leftMin"] = 1e20; chart["leftMax"] = 1e20+20000;
        const auto large = normalizeChartLayout({chart}).first().toMap();
        const auto axis = ChartMath::niceAxis(large["leftMin"].toDouble(), large["leftMax"].toDouble());
        QCOMPARE(axis.ticks.size(), 2);
    }
};
QTEST_GUILESS_MAIN(ChartAnalysisTest)
#include "test_chart_analysis.moc"
