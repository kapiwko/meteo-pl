#pragma once
#include "Fixture.hpp"
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

class HttpFixture : public QTcpServer {
public:
    QList<QByteArray> requests;
    int status = 200;
    QByteArray forecast = forecastFixture();
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
                    const auto body = buffer.startsWith("GET ") ? available : forecast;
                    socket->write("HTTP/1.1 " + QByteArray::number(status) + " Result\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    QUrl endpoint() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(serverPort())); }
};

