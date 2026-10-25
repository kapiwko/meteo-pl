#pragma once
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
class QNetworkReply;

class LegacyMeteogramModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QUrl lookup READ lookup WRITE setLookup NOTIFY changed)
    Q_PROPERTY(QUrl source READ source NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit LegacyMeteogramModel(QObject *parent = nullptr, QUrl endpoint = QUrl(QStringLiteral("https://old.meteo.pl")));
    QUrl lookup() const { return m_lookup; }
    QUrl source() const { return m_source; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    void setLookup(const QUrl &url);
    Q_INVOKABLE void reload();
signals:
    void changed();
private:
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_delay;
    QUrl m_lookup, m_source, m_endpoint;
    QString m_error;
    bool m_busy = false;
    quint64 m_generation = 0;
};
