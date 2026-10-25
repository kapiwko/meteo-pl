#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QtQml/qqmlregistration.h>

class ResponsiveImages : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(qreal maximumWidth READ maximumWidth WRITE setMaximumWidth NOTIFY maximumWidthChanged)
public:
    explicit ResponsiveImages(QObject *parent = nullptr) : QObject(parent) {}
    QQuickTextDocument *document() const { return m_document; }
    qreal maximumWidth() const { return m_width; }
    void setDocument(QQuickTextDocument *document);
    void setMaximumWidth(qreal width);
signals:
    void documentChanged();
    void maximumWidthChanged();
private:
    void schedule();
    void resizeImages();
    QPointer<QQuickTextDocument> m_document;
    QList<QMetaObject::Connection> m_connections;
    qreal m_width = 0;
    bool m_pending = false;
};
