#pragma once

#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>
#include <functional>

class QNetworkReply;

namespace Meteo {
class MeteoClient : public QObject {
    Q_OBJECT
public:
    explicit MeteoClient(QObject *parent = nullptr, QUrl endpoint = QUrl(QStringLiteral("https://devmgramapi.meteo.pl")));
    void fetch(double latitude, double longitude, const QString &model, qint64 requestedRun = 0);
    void cancel();

signals:
    void received(const QByteArray &json);
    void failed(const QString &message);
    void runsReceived(const QList<qint64> &runs);

private:
    void readReply(QNetworkReply *reply, quint64 generation, const std::function<void(QByteArray)> &consume);
    QNetworkAccessManager m_network;
    QUrl m_endpoint;
    QPointer<QNetworkReply> m_reply;
    quint64 m_generation = 0;
};
} // namespace Meteo
