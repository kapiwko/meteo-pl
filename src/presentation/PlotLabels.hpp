#pragma once
#include "MeteoUiExport.hpp"
#include <QFont>
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Rasterize only annotations, using the same glyph outlines as Canvas.
// Series and grid geometry continue to be submitted directly to the GPU.
class METEO_UI_EXPORT PlotLabels : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList labels READ labels WRITE setLabels NOTIFY labelsChanged)
    Q_PROPERTY(QFont font MEMBER m_font NOTIFY fontChanged)
public:
    explicit PlotLabels(QQuickItem *parent = nullptr);
    QVariantList labels() const { return m_labels; }
    void setLabels(const QVariantList &labels);
    void paint(QPainter *painter) override;
signals:
    void labelsChanged();
    void fontChanged();
private:
    QVariantList m_labels;
    QFont m_font;
};
