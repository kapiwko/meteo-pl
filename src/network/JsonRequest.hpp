#pragma once
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>

class QNetworkReply;
namespace Meteo {
class JsonRequest : public QObject {
    Q_OBJECT
public:
    explicit JsonRequest(QObject *parent = nullptr);
    void get(const QUrl &url, int cacheSeconds = 0, bool force = false);
    void cancel();
    int statusCode() const { return m_statusCode; }
signals:
    void received(const QJsonDocument &document);
    void failed(const QString &message);
private:
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    quint64 m_generation = 0;
    int m_statusCode = 0;
};
}
