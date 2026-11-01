#pragma once
#include <QPointF>
#include <QRectF>
#include <QColor>
#include <QVariantList>
#include <QVector>

// Bounded geometry builder, independent of Qt Quick and the rendering thread.
class PlotGeometry {
public:
    struct Vertex { float x,y; unsigned char r,g,b,a; };
    static constexpr qsizetype maximumVertices = 600000;
    static constexpr int maximumDashSteps = 65536;
    QVector<Vertex> vertices;
    QRectF clip;
    bool clipped = false;
    int dashSteps = 0;
    void clear() { vertices.clear(); dashSteps=0; }
    static QVector<qreal> dashPattern(const QVariantList &input);
    void triangle(QPointF a,QPointF b,QPointF c,const QColor &value);
    void polygon(const QVector<QPointF> &points,const QColor &value);
    void segment(QPointF a,QPointF b,const QColor &value,qreal lineWidth);
    void stroke(const QVector<QVector<QPointF>> &paths,const QVector<qreal> &dash,const QColor &value,qreal lineWidth);
};
