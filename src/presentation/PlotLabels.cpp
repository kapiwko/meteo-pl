#include "PlotLabels.hpp"
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

PlotLabels::PlotLabels(QQuickItem *parent) : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &PlotLabels::fontChanged, this, [this] { update(); });
}

void PlotLabels::setLabels(const QVariantList &labels)
{
    if (m_labels == labels) return;
    m_labels = labels;
    emit labelsChanged();
    update();
}

void PlotLabels::paint(QPainter *painter)
{
    const QFontMetrics metrics(m_font);
    for (const auto &entry : m_labels) {
        const auto label = entry.toMap();
        const auto text = label.value("text").toString();
        const auto align = label.value("align").toString();
        const auto baseline = label.value("baseline").toString();
        const auto advance = metrics.horizontalAdvance(text);
        const auto xOffset = align == "center" ? advance / 2 : align == "right" ? advance : 0;
        const auto yOffset = baseline == "middle" ? (metrics.ascent() / 2) + metrics.height() - metrics.ascent()
            : baseline == "top" || baseline == "hanging" ? 0
            : baseline == "bottom" ? metrics.height() : metrics.ascent();
        QPainterPath glyphs;
        glyphs.addText(label.value("x").toReal() - xOffset,
            label.value("y").toReal() - yOffset + metrics.ascent(), m_font, text);
        painter->fillPath(glyphs, label.value("color").value<QColor>());
    }
}
