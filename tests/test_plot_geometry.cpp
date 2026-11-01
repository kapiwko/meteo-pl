#include "PlotGeometry.hpp"
#include <QTest>
#include <limits>

class PlotGeometryTest : public QObject {
    Q_OBJECT
private slots:
    void boundsDashWorkBeforeTessellation()
    {
        PlotGeometry geometry;
        geometry.clipped=true; geometry.clip={0,0,500,100};
        const auto dash=PlotGeometry::dashPattern({1e-9,0,std::numeric_limits<double>::infinity()});
        QCOMPARE(dash,QVector<qreal>({0.5,0.5}));
        geometry.stroke({{{-1e9,50},{1e9,50}}},dash,Qt::black,1);
        QVERIFY(geometry.dashSteps<1100);
        QVERIFY(!geometry.vertices.isEmpty());
        QVERIFY(geometry.vertices.size()<4000);
        for (const auto &vertex : geometry.vertices) QVERIFY(vertex.x>=0 && vertex.x<=500);
        geometry.clear(); geometry.clipped=false;
        geometry.stroke({{{0,50},{1e15,50}}},dash,Qt::black,1);
        QVERIFY(geometry.dashSteps<=PlotGeometry::maximumDashSteps+1);
        QVERIFY(geometry.vertices.size()<=PlotGeometry::maximumVertices);
    }
    void keepsOrdinaryDashPhaseAfterClipping()
    {
        PlotGeometry geometry; geometry.clipped=true; geometry.clip={0,0,20,10};
        geometry.stroke({{{-5,5},{20,5}}},PlotGeometry::dashPattern({2,2}),Qt::black,1);
        QVERIFY(!geometry.vertices.isEmpty());
        // Original dash [-1,1] is clipped at x=0, not restarted at the border.
        QCOMPARE(geometry.vertices.first().x,0.f);
        bool firstEnd=false;
        for (const auto &point : geometry.vertices) if (point.x==1) firstEnd=true;
        QVERIFY(firstEnd);
    }
};
QTEST_GUILESS_MAIN(PlotGeometryTest)
#include "test_plot_geometry.moc"
