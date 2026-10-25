#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <functional>

class ServiceFixture : public QTcpServer {
public:
    QList<QUrl> requests;
    std::function<QByteArray(const QUrl &)> response;
    int status = 200;
    QByteArray redirect;
    ServiceFixture()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket, data = QByteArray()]() mutable {
                data += socket->readAll();
                if (!data.contains("\r\n\r\n")) return;
                const QUrl url = QUrl::fromEncoded(data.split(' ').value(1));
                requests.append(url);
                const auto body = response(url);
                const auto location = redirect.isEmpty() ? QByteArray() : "Location: " + redirect + "\r\n";
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " Response\r\n" + location + "Content-Type: application/json\r\nConnection: close\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
    }
    QUrl endpoint() const { return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(serverPort())); }
};
