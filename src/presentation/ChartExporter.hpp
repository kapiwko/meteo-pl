#pragma once
#include <QImage>
#include <QPointer>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class ChartExporter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
public:
    explicit ChartExporter(QObject *parent = nullptr);
    ~ChartExporter() override;
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    Q_INVOKABLE bool savePng(const QUrl &file, const QVariantList &items, const QVariantMap &metadata);
    Q_INVOKABLE QUrl suggestedFile(const QString &location, const QString &chart, const QString &date) const;
    Q_INVOKABLE void cancel();
signals:
    void stateChanged();
    void saved(const QUrl &file, const QString &path);
    void failed(const QString &message);
private:
    struct Capture { QPointer<QQuickItem> item; QSize size; QVariant exportMode; };
    void captureNext();
    void fail(const QString &message);
    void restore();
    QList<Capture> m_items;
    QList<QMetaObject::Connection> m_connections;
    QSharedPointer<QQuickItemGrabResult> m_grab;
    QImage m_image;
    QUrl m_file;
    QTimer m_deadline;
    QString m_error;
    int m_index = 0, m_y = 0;
    quint64 m_generation = 0;
    bool m_busy = false;
};
