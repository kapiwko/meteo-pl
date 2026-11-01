#include "PlotGeometry.hpp"
#include "DrawingPolicy.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
qreal cross(QPointF a, QPointF b, QPointF c) { return (b.x()-a.x())*(c.y()-a.y())-(b.y()-a.y())*(c.x()-a.x()); }
bool finite(QPointF p) { return std::isfinite(p.x()) && std::isfinite(p.y()); }
}
void PlotGeometry::triangle(QPointF a,QPointF b,QPointF c,const QColor &value)
{
    if (vertices.size() >= maximumVertices) return;
    if (!finite(a)||!finite(b)||!finite(c)||value.alpha()==0) return;
    QVector<QPointF> points{a,b,c};
    if (clipped) {
        if (clip.isEmpty()) return;
        for (int edge=0;edge<4;++edge) {
            QVector<QPointF> output;
            const auto inside=[&](QPointF p) { return edge==0 ? p.x()>=clip.left() : edge==1 ? p.x()<=clip.right() : edge==2 ? p.y()>=clip.top() : p.y()<=clip.bottom(); };
            const auto intersection=[&](QPointF p,QPointF q) {
                const bool vertical=edge<2;
                const qreal boundary=edge==0?clip.left():edge==1?clip.right():edge==2?clip.top():clip.bottom();
                const auto denominator=vertical?q.x()-p.x():q.y()-p.y();
                const auto t=std::abs(denominator)>1e-12?(boundary-(vertical?p.x():p.y()))/denominator:0;
                return p+(q-p)*t;
            };
            if (points.isEmpty()) return;
            auto previous=points.last();
            for (auto p:points) {
                if (inside(p)!=inside(previous)) output.append(intersection(previous,p));
                if (inside(p)) output.append(p);
                previous=p;
            }
            points=output;
        }
    }
    const auto alpha=value.alpha();
    const auto vertex=[&](QPointF p) { return Vertex{float(p.x()),float(p.y()),static_cast<unsigned char>(value.red()*alpha/255),static_cast<unsigned char>(value.green()*alpha/255),static_cast<unsigned char>(value.blue()*alpha/255),static_cast<unsigned char>(alpha)}; };
    for (qsizetype i=1;i+1<points.size() && vertices.size()+3<=maximumVertices;++i) { vertices.append(vertex(points[0])); vertices.append(vertex(points[i])); vertices.append(vertex(points[i+1])); }
}
void PlotGeometry::polygon(const QVector<QPointF> &input,const QColor &value)
{
    // Area segments touching their baseline contain repeated corners. Remove
    // consecutive duplicates before ear clipping, just as a Canvas fill does.
    QVector<QPointF> p;
    p.reserve(input.size());
    for (const auto &point : input)
        if (p.isEmpty() || p.last()!=point) p.append(point);
    if (p.size()>1 && p.first()==p.last()) p.removeLast();
    if (p.size()<3) return;
    qreal area=0;
    for (qsizetype i=0;i<p.size();++i) area+=p[i].x()*p[(i+1)%p.size()].y()-p[(i+1)%p.size()].x()*p[i].y();
    if (std::abs(area)<1e-10) return;
    const auto sign=area>0?1:-1;
    QVector<int> indices(p.size()); std::iota(indices.begin(),indices.end(),0);
    while (indices.size()>2) {
        bool found=false;
        for (qsizetype i=0;i<indices.size();++i) {
            const auto a=p[indices[(i+indices.size()-1)%indices.size()]], b=p[indices[i]], c=p[indices[(i+1)%indices.size()]];
            if (cross(a,b,c)*sign<=1e-10) continue;
            bool contains=false;
            for (qsizetype j=0;j<indices.size();++j) {
                if (j==i || j==(i+1)%indices.size() || j==(i+indices.size()-1)%indices.size()) continue;
                const auto q=p[indices[j]];
                if (cross(a,b,q)*sign>=0 && cross(b,c,q)*sign>=0 && cross(c,a,q)*sign>=0) { contains=true; break; }
            }
            if (contains) continue;
            triangle(a,b,c,value); indices.removeAt(i); found=true; break;
        }
        if (!found) break;
    }
}
void PlotGeometry::segment(QPointF a,QPointF b,const QColor &value, qreal lineWidth)
{
    const auto delta=b-a; const auto length=std::hypot(delta.x(),delta.y());
    if (length<1e-8 || !std::isfinite(length)) return;
    const QPointF normal(-delta.y()/length*lineWidth/2,delta.x()/length*lineWidth/2);
    triangle(a+normal,b+normal,b-normal,value); triangle(a+normal,b-normal,a-normal,value);
}
QVector<qreal> PlotGeometry::dashPattern(const QVariantList &input)
{
    return DrawingPolicy::dashPattern(input);
}

void PlotGeometry::stroke(const QVector<QVector<QPointF>> &paths,const QVector<qreal> &dash,const QColor &value,qreal lineWidth)
{
    const qreal period=std::accumulate(dash.begin(),dash.end(),0.0);
    for (const auto &path:paths) {
        qreal phase=0;
        for (qsizetype i=1;i<path.size();++i) {
            const auto a=path[i-1],b=path[i],delta=b-a;
            const auto length=std::hypot(delta.x(),delta.y());
            if (!std::isfinite(length) || length<1e-8) continue;
            qreal first=0,last=1;
            // Clip the centre line before expanding potentially long dashes.
            if (clipped) {
                const auto bounds=clip.adjusted(-lineWidth,-lineWidth,lineWidth,lineWidth);
                const auto bound=[&](qreal p,qreal q) {
                    if (p==0) return q>=0;
                    const auto t=q/p;
                    if (p<0) first=std::max(first,t); else last=std::min(last,t);
                    return first<=last;
                };
                if (!bound(-delta.x(),a.x()-bounds.left()) || !bound(delta.x(),bounds.right()-a.x())
                    || !bound(-delta.y(),a.y()-bounds.top()) || !bound(delta.y(),bounds.bottom()-a.y())) {
                    if (period>0) phase=std::fmod(phase+std::fmod(length,period),period);
                    continue;
                }
            }
            qreal position=first*length,end=last*length;
            if (dash.isEmpty() || dashSteps>=maximumDashSteps) {
                segment(a+delta*first,a+delta*last,value,lineWidth);
            } else {
                auto offset=std::fmod(phase+std::fmod(position,period),period);
                qsizetype index=0;
                while (offset>=dash[index]) { offset-=dash[index]; index=(index+1)%dash.size(); }
                while (position<end && dashSteps<maximumDashSteps) {
                    ++dashSteps;
                    const auto count=std::min(end-position,dash[index]-offset);
                    const auto next=position+count;
                    if (!(next>position)) break;
                    if (!(index%2)) segment(a+delta*(position/length),a+delta*(next/length),value,lineWidth);
                    position=next; offset=0; index=(index+1)%dash.size();
                }
                // A bounded solid remainder is the defined fallback.
                if (position<end) segment(a+delta*(position/length),a+delta*last,value,lineWidth);
            }
            if (period>0) phase=std::fmod(phase+std::fmod(length,period),period);
            if (vertices.size()>=maximumVertices) return;
        }
    }
}
