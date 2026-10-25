#include "ChartAnalysis.hpp"
#include "ChartConfiguration.hpp"
#include <QTest>
#include <cmath>
#include <limits>

class ChartAnalysisTest : public QObject {
    Q_OBJECT
private slots:
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
