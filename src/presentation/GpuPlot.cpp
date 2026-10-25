#include "GpuPlot.hpp"
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QSGVertexColorMaterial>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

namespace {
qreal cross(QPointF a, QPointF b, QPointF c) { return (b.x()-a.x())*(c.y()-a.y())-(b.y()-a.y())*(c.x()-a.x()); }
bool finite(QPointF p) { return std::isfinite(p.x()) && std::isfinite(p.y()); }
}
GpuPlot::GpuPlot(QQuickItem *parent) : QQuickItem(parent) { setFlag(ItemHasContents); }
void GpuPlot::requestPaint()
{
    if (m_pending) return;
    m_pending = true;
    QTimer::singleShot(0, this, [this] {
        m_pending = false;
        emit paint();
        ++m_frameCount;
        emit frameCountChanged();
        if (m_previousLabels != m_labels) { m_previousLabels = m_labels; emit textLabelsChanged(); }
        update();
    });
}
void GpuPlot::reset()
{
    m_vertices.clear(); m_labels.clear(); m_paths.clear(); m_stack.clear(); m_dash.clear();
    m_transform.reset(); m_clipped = false;
    m_fill = m_stroke = Qt::black; m_alpha = m_lineWidth = 1;
    m_font = "10px sans-serif"; m_textAlign = "left"; m_textBaseline = "alphabetic";
}
void GpuPlot::save() { m_stack.append({m_fill,m_stroke,m_alpha,m_lineWidth,m_font,m_textAlign,m_textBaseline,m_transform,m_clip,m_clipped,m_dash}); }
void GpuPlot::restore()
{
    if (m_stack.isEmpty()) return;
    const auto s = m_stack.takeLast();
    m_fill=s.fill; m_stroke=s.stroke; m_alpha=s.alpha; m_lineWidth=s.lineWidth;
    m_font=s.font; m_textAlign=s.align; m_textBaseline=s.baseline;
    m_transform=s.transform; m_clip=s.clip; m_clipped=s.clipped; m_dash=s.dash;
}
QColor GpuPlot::color(const QColor &value) const { auto result=value; result.setAlphaF(std::clamp(value.alphaF()*m_alpha,0.,1.)); return result; }
void GpuPlot::beginPath() { m_paths.clear(); }
void GpuPlot::moveTo(qreal x,qreal y) { m_paths.append(QVector<QPointF>{m_transform.map(QPointF(x,y))}); }
void GpuPlot::lineTo(qreal x,qreal y) { if (m_paths.isEmpty()) moveTo(x,y); else m_paths.last().append(m_transform.map(QPointF(x,y))); }
void GpuPlot::closePath() { if (!m_paths.isEmpty() && !m_paths.last().isEmpty()) m_paths.last().append(m_paths.last().first()); }
void GpuPlot::rect(qreal x,qreal y,qreal w,qreal h) { moveTo(x,y); lineTo(x+w,y); lineTo(x+w,y+h); lineTo(x,y+h); closePath(); }
void GpuPlot::arc(qreal x,qreal y,qreal radius,qreal start,qreal end)
{
    if (!(radius>0) || !std::isfinite(radius)) return;
    const auto span=std::clamp(end-start,-2*std::numbers::pi,2*std::numbers::pi);
    const int count=std::clamp(int(std::ceil(std::abs(span)*std::sqrt(radius)*3)),12,128);
    for (int i=0;i<=count;++i) {
        const auto angle=start+span*i/count;
        const auto point=QPointF(x+radius*std::cos(angle),y+radius*std::sin(angle));
        if (!i && m_paths.isEmpty()) moveTo(point.x(),point.y()); else lineTo(point.x(),point.y());
    }
}
void GpuPlot::triangle(QPointF a,QPointF b,QPointF c,const QColor &value)
{
    if (!finite(a)||!finite(b)||!finite(c)||value.alpha()==0) return;
    QVector<QPointF> points{a,b,c};
    if (m_clipped) {
        if (m_clip.isEmpty()) return;
        for (int edge=0;edge<4;++edge) {
            QVector<QPointF> output;
            const auto inside=[&](QPointF p) { return edge==0 ? p.x()>=m_clip.left() : edge==1 ? p.x()<=m_clip.right() : edge==2 ? p.y()>=m_clip.top() : p.y()<=m_clip.bottom(); };
            const auto intersection=[&](QPointF p,QPointF q) {
                const bool vertical=edge<2;
                const qreal boundary=edge==0?m_clip.left():edge==1?m_clip.right():edge==2?m_clip.top():m_clip.bottom();
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
    for (qsizetype i=1;i+1<points.size();++i) { m_vertices.append(vertex(points[0])); m_vertices.append(vertex(points[i])); m_vertices.append(vertex(points[i+1])); }
}
void GpuPlot::polygon(const QVector<QPointF> &input,const QColor &value)
{
    auto p=input;
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
void GpuPlot::fill() { for (const auto &path:m_paths) polygon(path,color(m_fill)); }
void GpuPlot::segment(QPointF a,QPointF b,const QColor &value)
{
    const auto delta=b-a; const auto length=std::hypot(delta.x(),delta.y());
    if (length<1e-8 || !std::isfinite(length)) return;
    const QPointF normal(-delta.y()/length*m_lineWidth/2,delta.x()/length*m_lineWidth/2);
    triangle(a+normal,b+normal,b-normal,value); triangle(a+normal,b-normal,a-normal,value);
}
void GpuPlot::stroke()
{
    const auto value=color(m_stroke);
    for (const auto &path:m_paths) {
        qsizetype dashIndex=0; qreal dashPosition=0;
        for (qsizetype i=1;i<path.size();++i) {
            const auto a=path[i-1], b=path[i]; const auto delta=b-a;
            const auto length=std::hypot(delta.x(),delta.y());
            if (m_dash.isEmpty()) { segment(a,b,value); continue; }
            if (length<1e-8 || !std::isfinite(length)) continue;
            qreal position=0;
            while (position<length) {
                const auto count=std::min(length-position,m_dash[dashIndex]-dashPosition);
                if (!(dashIndex%2)) segment(a+delta*(position/length),a+delta*((position+count)/length),value);
                position+=count; dashPosition+=count;
                if (dashPosition>=m_dash[dashIndex]-1e-8) { dashIndex=(dashIndex+1)%m_dash.size(); dashPosition=0; }
            }
        }
    }
}
void GpuPlot::fillRect(qreal x,qreal y,qreal w,qreal h) { polygon({m_transform.map(QPointF(x,y)),m_transform.map(QPointF(x+w,y)),m_transform.map(QPointF(x+w,y+h)),m_transform.map(QPointF(x,y+h))},color(m_fill)); }
void GpuPlot::strokeRect(qreal x,qreal y,qreal w,qreal h) { const auto paths=m_paths; beginPath();rect(x,y,w,h);stroke();m_paths=paths; }
void GpuPlot::clip()
{
    QRectF bounds;
    bool first=true;
    for (const auto &path:m_paths) for (const auto &p:path) { if (first) { bounds=QRectF(p,QSizeF()); first=false; } else { bounds.setLeft(std::min(bounds.left(),p.x()));bounds.setRight(std::max(bounds.right(),p.x()));bounds.setTop(std::min(bounds.top(),p.y()));bounds.setBottom(std::max(bounds.bottom(),p.y())); } }
    m_clip=m_clipped?m_clip.intersected(bounds):bounds;m_clipped=true;
}
void GpuPlot::translate(qreal x,qreal y) { m_transform.translate(x,y); }
void GpuPlot::rotate(qreal radians) { m_transform.rotateRadians(radians); }
void GpuPlot::setLineDash(const QVariantList &dash) { m_dash.clear();for (const auto &v:dash) if (v.toDouble()>0) m_dash.append(v.toDouble());if (m_dash.size()%2) m_dash+=m_dash; }
void GpuPlot::fillText(const QString &text,qreal x,qreal y)
{
    const auto point=m_transform.map(QPointF(x,y));
    m_labels.append(QVariantMap{{"text",text},{"x",point.x()},{"y",point.y()},{"color",color(m_fill)}, {"align",m_textAlign},{"baseline",m_textBaseline}});
}
QSGNode *GpuPlot::updatePaintNode(QSGNode *old,UpdatePaintNodeData *)
{
    if (window()->rendererInterface()->graphicsApi()==QSGRendererInterface::Software) {
        delete old;
        if (!m_softwareReported) { m_softwareReported=true; QMetaObject::invokeMethod(this,[this]{emit softwareBackend();},Qt::QueuedConnection); }
        return nullptr;
    }
    // Qt Quick batches geometry with 16-bit indices. A dense chart can exceed
    // that limit even without an index buffer of its own (e.g. cloud circles).
    // Keep each node below the limit and split only at triangle boundaries.
    constexpr qsizetype chunkSize = 60000;
    auto *root = old ? old : new QSGNode;
    auto *child = root->firstChild();
    for (qsizetype first=0; first<m_vertices.size(); first+=chunkSize) {
        auto *node=static_cast<QSGGeometryNode *>(child);
        if (!node) {
            node=new QSGGeometryNode;
            auto *geometry=new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(),0);
            geometry->setDrawingMode(QSGGeometry::DrawTriangles);
            node->setGeometry(geometry);node->setFlag(QSGNode::OwnsGeometry);
            node->setMaterial(new QSGVertexColorMaterial);node->setFlag(QSGNode::OwnsMaterial);
            root->appendChildNode(node);
        }
        const auto count=std::min(chunkSize,m_vertices.size()-first);
        auto *geometry=node->geometry();geometry->allocate(count);
        auto *vertices=geometry->vertexDataAsColoredPoint2D();
        for (qsizetype i=0;i<count;++i) { const auto &v=m_vertices[first+i];vertices[i].set(v.x,v.y,v.r,v.g,v.b,v.a); }
        node->markDirty(QSGNode::DirtyGeometry);
        child=node->nextSibling();
    }
    while (child) {
        auto *next=child->nextSibling();root->removeChildNode(child);delete child;child=next;
    }
    return root;
}
