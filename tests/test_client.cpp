#include "network/MeteoClient.hpp"
#include "Fixture.hpp"
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

class HttpFixture : public QTcpServer {
public:
    QList<QByteArray> requests;
    int status = 200;
    QByteArray available = R"({"um4_60":[100,300,200]})";
    HttpFixture()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer = QByteArray()]() mutable {
                    buffer += socket->readAll();
                    const auto headerEnd = buffer.indexOf("\r\n\r\n");
                    if (headerEnd < 0) return;
                    int contentLength = 0;
                    for (const auto &line : buffer.left(headerEnd).split('\n'))
                        if (line.toLower().startsWith("content-length:")) contentLength = line.mid(15).trimmed().toInt();
                    if (buffer.size() < headerEnd + 4 + contentLength) return;
                    requests.append(buffer);
                    const auto body = buffer.startsWith("GET ") ? available : forecastFixture();
                    socket->write("HTTP/1.1 " + QByteArray::number(status) + " Result\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    QUrl endpoint() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(serverPort())); }
};

class ClientTest : public QObject {
    Q_OBJECT
private slots:
    void requestsNewestRunAndCoordinates()
    {
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Meteo::MeteoClient client(nullptr, server.endpoint());
        QSignalSpy received(&client, &Meteo::MeteoClient::received);
        QSignalSpy failed(&client, &Meteo::MeteoClient::failed);
        client.fetch(50.8661, 20.6286, "um4_60");
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(failed.size(), 0);
        QCOMPARE(server.requests.size(), 2);
        const auto request = server.requests[1];
        QVERIFY(request.startsWith("POST /meteorograms/um4_60 "));
        const auto payload = QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(payload["date"].toInteger(), 300);
        QCOMPARE(payload["point"].toObject()["lat"].toString(), "50.866100");
    }
    void reportsHttpFailure()
    {
        HttpFixture server;
        server.status = 503;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Meteo::MeteoClient client(nullptr, server.endpoint());
        QSignalSpy failed(&client, &Meteo::MeteoClient::failed);
        client.fetch(52, 21, "um4_60");
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(server.requests.size(), 1);
        QVERIFY(failed.first().first().toString().contains("503"));
    }
    void rejectsMissingModel()
    {
        HttpFixture server;
        server.available = "{}";
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Meteo::MeteoClient client(nullptr, server.endpoint());
        QSignalSpy failed(&client, &Meteo::MeteoClient::failed);
        client.fetch(52, 21, "um4_60");
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(server.requests.size(), 1);
    }
    void cancelsPreviousLocation()
    {
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Meteo::MeteoClient client(nullptr, server.endpoint());
        QSignalSpy received(&client, &Meteo::MeteoClient::received);
        QSignalSpy failed(&client, &Meteo::MeteoClient::failed);
        client.fetch(52, 21, "um4_60");
        client.cancel();
        client.fetch(50.8661, 20.6286, "um4_60");
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(failed.size(), 0);
    }
};
QTEST_GUILESS_MAIN(ClientTest)
#include "test_client.moc"
