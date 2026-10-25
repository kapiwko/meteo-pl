#include "ImageCache.hpp"
#include <QBuffer>
#include <QCoreApplication>
#include <QImageReader>
#include <QNetworkReply>
#include <QRunnable>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>

namespace Meteo {
namespace {
bool allowsReuse(const QByteArray &header)
{
    const auto control = header.toLower();
    static const QRegularExpression immediatelyStale(QStringLiteral(R"((?:^|,)\s*max-age\s*=\s*"?0"?\s*(?:,|$))"));
    return !control.contains("no-store") && !control.contains("no-cache")
        && !immediatelyStale.match(QString::fromLatin1(control)).hasMatch();
}
}
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
void ImageCache::clear()
{
    ++m_generation;
    m_images.clear();
    m_disk->clear();
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
    const auto generation = m_generation;
    QNetworkRequest request(url);
    request.setTransferTimeout(20000);
    const auto stored = m_disk->metaData(url);
    bool fresh = stored.isValid() && stored.expirationDate() > QDateTime::currentDateTimeUtc();
    for (const auto &header : stored.rawHeaders())
        if (header.first.compare("Cache-Control", Qt::CaseInsensitive) == 0 && !allowsReuse(header.second)) fresh = false;
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
        fresh ? QNetworkRequest::PreferCache : QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = m_network.get(request);
    connect(reply, &QIODevice::readyRead, this, [reply] {
        if (reply->bytesAvailable() > 8 * 1024 * 1024) reply->abort();
    });
    // A total deadline also covers replies that trickle bytes without finishing.
    QTimer::singleShot(25000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, generation] {
        if (generation != m_generation) m_disk->remove(reply->url());
        const auto bytes = reply->readAll();
        const auto error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        const auto now = QDateTime::currentDateTimeUtc();
        const auto metadata = m_disk->metaData(reply->url());
        const auto cacheControl = reply->rawHeader("Cache-Control").toLower();
        const bool cacheable = allowsReuse(cacheControl);
        const bool needsExpiry = cacheable && !reply->hasRawHeader("Expires") && !cacheControl.contains("max-age");
        const auto expiry = needsExpiry ? now.addSecs(1800)
            : metadata.expirationDate().isValid() ? metadata.expirationDate() : now;
        QNetworkCacheMetaData fallback;
        if (needsExpiry) {
            // old.meteo.pl supplies Last-Modified but no freshness lifetime.
            // Qt otherwise revalidates every frame once it leaves the RAM LRU.
            // Persist the same bounded 30-minute lifetime as the decoded image.
            fallback.setUrl(QUrl(key));
            fallback.setExpirationDate(expiry);
            fallback.setSaveToDisk(true);
            auto headers = reply->rawHeaderPairs();
            headers.append(qMakePair(QByteArray("Cache-Control"), QByteArray("private, max-age=1800")));
            fallback.setRawHeaders(headers);
            fallback.setAttributes({{QNetworkRequest::HttpStatusCodeAttribute, reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)},
                {QNetworkRequest::HttpReasonPhraseAttribute, reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute)}});
        }
        reply->deleteLater();
        // PNG decompression must not block input or the map's current frame.
        m_decoder.start(QRunnable::create([this, bytes, error, key, expiry, cacheable, fallback, generation] {
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
            QMetaObject::invokeMethod(this, [this, key, image, failure, expiry, cacheable, fallback, bytes, generation] {
                if (!image.isNull() && cacheable && generation == m_generation) {
                    if (fallback.isValid()) {
                        if (auto *device = m_disk->prepare(fallback)) {
                            if (device->write(bytes) == bytes.size()) m_disk->insert(device);
                            else delete device;
                        }
                    }
                    m_images.insert(key, new Entry{image, expiry}, int(image.sizeInBytes()));
                }
                const auto callbacks = m_pending.take(key);
                for (const auto &callback : callbacks) callback(image, failure);
            }, Qt::QueuedConnection);
        }));
    });
}
}
