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

GpuPlot::GpuPlot(QQuickItem *parent) : QQuickItem(parent) { setFlag(ItemHasContents); }
QHash<int, QByteArray> PlotTextModel::roleNames() const
{
    return {{Qt::UserRole, "labelText"}, {Qt::UserRole + 1, "labelColor"}, {Qt::UserRole + 2, "labelX"},
        {Qt::UserRole + 3, "labelY"}, {Qt::UserRole + 4, "labelAlign"}, {Qt::UserRole + 5, "labelBaseline"}};
}
QVariant PlotTextModel::data(const QModelIndex &index, int role) const
{
    static const QStringList keys{"text", "color", "x", "y", "align", "baseline"};
    const auto key = role - Qt::UserRole;
    if (!index.isValid() || index.row() < 0 || index.row() >= m_labels.size() || key < 0 || key >= keys.size()) return {};
    return m_labels[index.row()].toMap().value(keys[key]);
}
void PlotTextModel::synchronize(const QVariantList &labels)
{
    if (m_labels == labels) return;
    const auto common = std::min(m_labels.size(), labels.size());
    for (int i = 0; i < common; ++i) {
        if (m_labels[i] == labels[i]) continue;
        m_labels[i] = labels[i];
        emit dataChanged(index(i), index(i));
    }
    if (m_labels.size() > labels.size()) {
        beginRemoveRows({}, labels.size(), m_labels.size() - 1);
        m_labels.resize(labels.size());
        endRemoveRows();
    } else if (m_labels.size() < labels.size()) {
        beginInsertRows({}, m_labels.size(), labels.size() - 1);
        for (auto i = m_labels.size(); i < labels.size(); ++i) m_labels.append(labels[i]);
        endInsertRows();
    }
}
void GpuPlot::requestPaint()
{
    if (m_pending) return;
    m_pending = true;
    polish();
}
void GpuPlot::updatePolish()
{
    if (!m_pending) return;
    m_pending = false;
    emit paint();
    ++m_frameCount;
    emit frameCountChanged();
    m_textModel.synchronize(m_labels);
    if (m_previousLabels != m_labels) { m_previousLabels = m_labels; emit textLabelsChanged(); }
    update();
}
void GpuPlot::reset()
{
    m_geometry.clear(); m_labels.clear(); m_paths.clear(); m_stack.clear(); m_dash.clear();
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
void GpuPlot::fill() { m_geometry.clip=m_clip; m_geometry.clipped=m_clipped; for (const auto &path:m_paths) m_geometry.polygon(path,color(m_fill)); }
void GpuPlot::stroke()
{
    m_geometry.clip=m_clip; m_geometry.clipped=m_clipped;
    m_geometry.stroke(m_paths,m_dash,color(m_stroke),m_lineWidth);
}
void GpuPlot::fillRect(qreal x,qreal y,qreal w,qreal h) { m_geometry.clip=m_clip; m_geometry.clipped=m_clipped; m_geometry.polygon({m_transform.map(QPointF(x,y)),m_transform.map(QPointF(x+w,y)),m_transform.map(QPointF(x+w,y+h)),m_transform.map(QPointF(x,y+h))},color(m_fill)); }
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
void GpuPlot::setLineDash(const QVariantList &dash) { m_dash = PlotGeometry::dashPattern(dash); }
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
    for (qsizetype first=0; first<m_geometry.vertices.size(); first+=chunkSize) {
        auto *node=static_cast<QSGGeometryNode *>(child);
        if (!node) {
            node=new QSGGeometryNode;
            auto *geometry=new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(),0);
            geometry->setDrawingMode(QSGGeometry::DrawTriangles);
            node->setGeometry(geometry);node->setFlag(QSGNode::OwnsGeometry);
            node->setMaterial(new QSGVertexColorMaterial);node->setFlag(QSGNode::OwnsMaterial);
            root->appendChildNode(node);
        }
        const auto count=std::min(chunkSize,m_geometry.vertices.size()-first);
        auto *geometry=node->geometry();geometry->allocate(count);
        auto *vertices=geometry->vertexDataAsColoredPoint2D();
        for (qsizetype i=0;i<count;++i) { const auto &v=m_geometry.vertices[first+i];vertices[i].set(v.x,v.y,v.r,v.g,v.b,v.a); }
        node->markDirty(QSGNode::DirtyGeometry);
        child=node->nextSibling();
    }
    while (child) {
        auto *next=child->nextSibling();root->removeChildNode(child);delete child;child=next;
    }
    return root;
}
