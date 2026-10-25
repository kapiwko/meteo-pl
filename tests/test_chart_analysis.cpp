#include "ChartAnalysis.hpp"
#include "ChartConfiguration.hpp"
#include <QTest>
#include <QFile>
#include <QJSEngine>
#include <cmath>
#include <limits>

class ChartAnalysisTest : public QObject {
    Q_OBJECT
private slots:
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
