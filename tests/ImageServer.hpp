#pragma once
#include <QBuffer>
#include <QImage>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <utility>

class ImageServer : public QTcpServer {
public:
    struct Request { QPointer<QTcpSocket> socket; QUrl url; };
    QList<Request> held;
    QList<QUrl> requests;
    bool hold = false;
    int statusCode = 200;
    QSize imageSize{256,256};
    QByteArray cacheControl = "public, max-age=86400";
    QByteArray lastModified;
    ImageServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket, data = QByteArray(), sent = false]() mutable {
                    data += socket->readAll();
                    if (sent || !data.contains("\r\n\r\n")) return;
                    sent = true;
                    const QUrl url = QUrl::fromEncoded(data.split(' ').value(1));
                    requests.append(url);
                    if (hold) held.append({socket, url}); else respond({socket, url});
                });
            }
        });
    }
    void respond(const Request &request)
    {
        if (!request.socket) return;
        const bool failure = statusCode != 200 || request.url.path().contains("error");
        const int code = failure ? (statusCode == 200 ? 404 : statusCode) : 200;
        const QColor color = request.url.path().contains("transparent") ? Qt::transparent
            : request.url.path().contains("blue") ? Qt::blue
            : request.url.path().contains("green") ? Qt::green : Qt::red;
        QImage image(imageSize, QImage::Format_ARGB32);
        image.fill(color);
        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        const auto body = failure ? QByteArray("missing") : buffer.data();
        request.socket->write("HTTP/1.1 " + QByteArray::number(code) + " Response"
            + "\r\nContent-Type: image/png\r\n"
            + (cacheControl.isEmpty() ? QByteArray() : "Cache-Control: " + cacheControl + "\r\n")
            + (lastModified.isEmpty() ? QByteArray() : "Last-Modified: " + lastModified + "\r\n")
            + "Connection: close\r\nContent-Length: "
            + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        request.socket->disconnectFromHost();
    }
    void releaseOne() { if (!held.isEmpty()) respond(held.takeFirst()); }
    void releaseAll() { const auto pending = std::exchange(held, {}); for (const auto &request : pending) respond(request); }
    QUrl url(const QString &path) const { return QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(serverPort()) + path); }
};
