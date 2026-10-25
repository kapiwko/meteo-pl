#pragma once
#include <QQuickItem>
#include <QColor>
#include <QTransform>
#include <QVariantList>
#include <QVector>
#include <QtQml/qqmlregistration.h>

// Records chart primitives on the GUI thread and submits colored triangles to
// Qt Quick's render thread. No intermediate chart image or QPainter is used.
class GpuPlot : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QColor fillStyle MEMBER m_fill)
    Q_PROPERTY(QColor strokeStyle MEMBER m_stroke)
    Q_PROPERTY(qreal globalAlpha MEMBER m_alpha)
    Q_PROPERTY(qreal lineWidth MEMBER m_lineWidth)
    Q_PROPERTY(QString font MEMBER m_font)
    Q_PROPERTY(QString textAlign MEMBER m_textAlign)
    Q_PROPERTY(QString textBaseline MEMBER m_textBaseline)
    Q_PROPERTY(QVariantList textLabels READ textLabels NOTIFY textLabelsChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY frameCountChanged)
public:
    explicit GpuPlot(QQuickItem *parent = nullptr);
    QVariantList textLabels() const { return m_labels; }
    int frameCount() const { return m_frameCount; }
    Q_INVOKABLE QObject *getContext(const QString &) { return this; }
    Q_INVOKABLE void requestPaint();
    Q_INVOKABLE void requestCursorPaint() { emit cursorPaintRequested(); }
    Q_INVOKABLE void reset();
    Q_INVOKABLE void save();
    Q_INVOKABLE void restore();
    Q_INVOKABLE void beginPath();
    Q_INVOKABLE void moveTo(qreal x, qreal y);
    Q_INVOKABLE void lineTo(qreal x, qreal y);
    Q_INVOKABLE void closePath();
    Q_INVOKABLE void rect(qreal x, qreal y, qreal width, qreal height);
    Q_INVOKABLE void arc(qreal x, qreal y, qreal radius, qreal start, qreal end);
    Q_INVOKABLE void fill();
    Q_INVOKABLE void stroke();
    Q_INVOKABLE void fillRect(qreal x, qreal y, qreal width, qreal height);
    Q_INVOKABLE void strokeRect(qreal x, qreal y, qreal width, qreal height);
    Q_INVOKABLE void clip();
    Q_INVOKABLE void translate(qreal x, qreal y);
    Q_INVOKABLE void rotate(qreal radians);
    Q_INVOKABLE void setLineDash(const QVariantList &dash);
    Q_INVOKABLE void fillText(const QString &text, qreal x, qreal y);
signals:
    void paint();
    void textLabelsChanged();
    void softwareBackend();
    void cursorPaintRequested();
    void frameCountChanged();
protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
private:
    struct Vertex { float x, y; unsigned char r, g, b, a; };
    struct State {
        QColor fill, stroke;
        qreal alpha, lineWidth;
        QString font, align, baseline;
        QTransform transform;
        QRectF clip;
        bool clipped;
        QVector<qreal> dash;
    };
    void triangle(QPointF a, QPointF b, QPointF c, const QColor &color);
    void polygon(const QVector<QPointF> &, const QColor &);
    void segment(QPointF a, QPointF b, const QColor &);
    QColor color(const QColor &value) const;
    QColor m_fill = Qt::black, m_stroke = Qt::black;
    qreal m_alpha = 1, m_lineWidth = 1;
    QString m_font = "10px sans-serif", m_textAlign = "left", m_textBaseline = "alphabetic";
    QTransform m_transform;
    QRectF m_clip;
    bool m_clipped = false, m_pending = false, m_softwareReported = false;
    int m_frameCount = 0;
    QVector<State> m_stack;
    QVector<qreal> m_dash;
    QVector<QVector<QPointF>> m_paths;
    QVector<Vertex> m_vertices;
    QVariantList m_labels, m_previousLabels;
};
