#include "ImageCache.hpp"
#include <QBuffer>
#include <QCoreApplication>
#include <QImageReader>
#include <QNetworkReply>
#include <QRunnable>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>

namespace Meteo {
ImageCache::ImageCache(const QString &directory, QObject *parent)
    : QObject(parent), m_disk(new QNetworkDiskCache(&m_network))
{
    m_disk->setCacheDirectory(directory);
    m_disk->setMaximumCacheSize(256 * 1024 * 1024);
    m_network.setCache(m_disk);
    m_decoder.setMaxThreadCount(2);
}
ImageCache *ImageCache::shared()
{
    static auto *cache = new ImageCache(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/meteo-pl/maps/") + QCoreApplication::applicationName().replace('/', '_'), qApp);
    return cache;
}
void ImageCache::request(const QUrl &url, Completion completion)
{
    const auto key = url.toString(QUrl::FullyEncoded);
    if (url.scheme() != "https" && url.scheme() != "http") {
        completion({}, QStringLiteral("Nieprawidłowy adres obrazu mapy."));
        return;
    }
    if (auto *entry = m_images.object(key); entry && entry->expires > QDateTime::currentDateTimeUtc()) {
        completion(entry->image, {});
        return;
    }
    if (m_pending.contains(key)) { m_pending[key].append(std::move(completion)); return; }
    m_pending.insert(key, {std::move(completion)});
    QNetworkRequest request(url);
    request.setTransferTimeout(20000);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_network.get(request);
    connect(reply, &QIODevice::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 8 * 1024 * 1024) reply->abort();
    });
    // A total deadline also covers replies that trickle bytes without finishing.
    QTimer::singleShot(25000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        const auto bytes = reply->readAll();
        const auto error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        const auto metadata = m_disk->metaData(reply->url());
        const auto expiry = metadata.expirationDate().isValid() ? metadata.expirationDate()
            : QDateTime::currentDateTimeUtc().addSecs(1800);
        const bool cacheable = !reply->rawHeader("Cache-Control").contains("no-store");
        reply->deleteLater();
        // PNG decompression must not block input or the map's current frame.
        m_decoder.start(QRunnable::create([this, bytes, error, key, expiry, cacheable] {
            QImage image;
            QString failure = error;
            if (failure.isEmpty()) {
                QBuffer buffer;
                buffer.setData(bytes);
                buffer.open(QIODevice::ReadOnly);
                QImageReader reader(&buffer);
                const auto size = reader.size();
                if (size.isValid() && qint64(size.width()) * size.height() <= 16 * 1024 * 1024)
                    image = reader.read();
                if (image.isNull()) failure = QStringLiteral("Nie udało się odczytać obrazu mapy.");
            }
            QMetaObject::invokeMethod(this, [this, key, image, failure, expiry, cacheable] {
                if (!image.isNull() && cacheable)
                    m_images.insert(key, new Entry{image, expiry}, int(image.sizeInBytes()));
                const auto callbacks = m_pending.take(key);
                for (const auto &callback : callbacks) callback(image, failure);
            }, Qt::QueuedConnection);
        }));
    });
}
}
