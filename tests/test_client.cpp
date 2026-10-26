#include "network/MeteoClient.hpp"
#include "network/JsonRequest.hpp"
#include <QElapsedTimer>
#include "ClientFixture.hpp"
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>


class ClientTest : public QObject {
    Q_OBJECT
private slots:
    void tricklingResponsesCannotExtendTheTotalDeadline_data()
    {
        QTest::addColumn<bool>("json");
        QTest::newRow("forecast") << false;
        QTest::newRow("json") << true;
    }
    void tricklingResponsesCannotExtendTheTotalDeadline()
    {
        QFETCH(bool,json);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int chunks = 0;
        connect(&server,&QTcpServer::newConnection,this,[&] {
            auto *socket = server.nextPendingConnection();
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket,sent=false]() mutable {
                socket->readAll();
                if (sent) return;
                sent = true;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 10000\r\n\r\n");
                auto *drip = new QTimer(socket);
                connect(drip,&QTimer::timeout,socket,[&,socket] { ++chunks; socket->write(" "); });
                drip->start(15);
            });
        });
        const QUrl url(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
        const Meteo::RequestPolicy policy{150,250,1024*1024};
        Meteo::JsonRequest request(nullptr,policy);
        Meteo::MeteoClient client(nullptr,url,policy);
        QSignalSpy failed(json ? static_cast<QObject *>(&request) : static_cast<QObject *>(&client),SIGNAL(failed(QString)));
        QElapsedTimer elapsed; elapsed.start();
        if (json) request.get(url); else client.fetch(52,21,"um4_60");
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(),1,1500);
        QVERIFY(chunks >= 3); // Actual progress prevented an inactivity timeout.
        QVERIFY(elapsed.elapsed() < 1200);
        QVERIFY(failed.first().first().toString().contains("timed out"));
        QTest::qWait(100);
        QCOMPARE(failed.count(),1);
    }
    void changingLocationFromTheRunListCannotPostTheOldQuery()
    {
        HttpFixture server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Meteo::MeteoClient client(nullptr,server.endpoint());
        bool switched=false;
        connect(&client,&Meteo::MeteoClient::runsReceived,this,[&] {
            if (switched) return;
            switched=true;
            client.fetch(50.8661,20.6286,"um4_60");
        });
        QSignalSpy received(&client,&Meteo::MeteoClient::received);
        client.fetch(52.2297,21.0122,"um4_60");
        QTRY_COMPARE(received.count(),1);
        QCOMPARE(server.requests.size(),3);
        const auto body=server.requests.last().mid(server.requests.last().indexOf("\r\n\r\n")+4);
        QCOMPARE(QJsonDocument::fromJson(body).object()["point"].toObject()["lat"].toString(),"50.866100");
    }
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
