#include "ChartAnalysis.hpp"
#include "ChartConfiguration.hpp"
#include "ChartDataBuilder.hpp"
#include <QTest>
#include <QFile>
#include <QJSEngine>
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
    engine.globalObject().setProperty("fieldsUnderTest", engine.toScriptValue(fields));
    return engine.evaluate(QStringLiteral(R"JS(
        (function() {
            const rectangles=[], polygons=[], circles=[], ctx={}; let path=[];
            for (const name of ["reset","save","restore","rect","clip","setLineDash","stroke","fillText","translate","rotate"])
                ctx[name]=function(){};
            ctx.beginPath=function(){path=[];};
            ctx.moveTo=ctx.lineTo=function(x,y){path.push([x,y]);};
            ctx.closePath=function(){path.closed=true;};
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
            return {rectangles:rectangles,polygons:polygons,circles:circles};
        })()
    )JS"));
}
}

class ChartAnalysisTest : public QObject {
    Q_OBJECT
private slots:
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
