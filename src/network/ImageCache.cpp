#include "core/Localization.hpp"
#include "ImageCache.hpp"
#include "RequestPolicy.hpp"
#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
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
QDateTime expirationFor(const QNetworkReply *reply, const QNetworkCacheMetaData &metadata, const QDateTime &now)
{
    if (metadata.expirationDate().isValid()) return metadata.expirationDate();
    static const QRegularExpression maxAge(QStringLiteral(R"((?:^|,)\s*max-age\s*=\s*"?(\d+))"));
    const auto match = maxAge.match(QString::fromLatin1(reply->rawHeader("Cache-Control").toLower()));
    if (match.hasMatch()) {
        bool ok = false;
        const auto seconds = match.captured(1).toLongLong(&ok);
        if (ok) return now.addSecs(std::min<qint64>(seconds, 7*86400));
    }
    const auto expires = QDateTime::fromString(QString::fromLatin1(reply->rawHeader("Expires")), Qt::RFC2822Date);
    return expires.isValid() ? expires : now.addSecs(1800);
}
}
ImageCache::ImageCache(const QString &directory, QObject *parent)
    : QObject(parent)
{
    const auto locks = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/meteo-pl-locks");
    if (QDir().mkpath(directory) && QDir().mkpath(locks)) {
        const auto identity = QFileInfo(directory).canonicalFilePath().toUtf8();
        m_diskLock = std::make_unique<QLockFile>(locks + '/' + QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex() + ".lock");
        m_diskLock->setStaleLockTime(0);
        if (m_diskLock->tryLock(0)) {
            m_disk = new QNetworkDiskCache(&m_network);
            m_disk->setCacheDirectory(directory);
            m_disk->setMaximumCacheSize(256 * 1024 * 1024);
            m_network.setCache(m_disk);
        }
    }
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
    if (m_disk) m_disk->clear();
}
void ImageCache::request(const QUrl &url, Completion completion, QObject *consumer, Priority priority)
{
    const auto key = url.toString(QUrl::FullyEncoded);
    if (url.scheme() != "https" && url.scheme() != "http") {
        completion({}, Meteo::i18n("Invalid map image URL."));
        return;
    }
    if (auto *entry = m_images.object(key); entry && entry->expires > QDateTime::currentDateTimeUtc()) {
        completion(entry->image, {});
        return;
    }
    int consumers = 0;
    for (const auto &pending : std::as_const(m_pending)) consumers += pending.subscribers.size();
    if ((!m_pending.contains(key) && m_pending.size() >= maximumPendingJobs) || consumers >= maximumSubscribers) {
        completion({}, Meteo::i18n("Too many images are waiting to load. Try again."));
        return;
    }
    if (consumer && !m_contexts.contains(consumer)) {
        m_contexts.insert(consumer, connect(consumer, &QObject::destroyed, this, [this,consumer] {
            cancelRequests(consumer);
            m_contexts.remove(consumer);
        }));
    }
    auto pending = m_pending.find(key);
    if (pending == m_pending.end()) {
        const auto token = ++m_nextToken;
        pending = m_pending.insert(key, Pending{{}, {}, token, priority});
        (priority == Priority::Visible ? m_visible : m_prefetch).enqueue({key,token});
    } else if (!pending->active && priority == Priority::Visible && pending->priority == Priority::Prefetch) {
        pending->priority = Priority::Visible;
        m_visible.enqueue({key,pending->token});
    }
    pending->subscribers.append({std::move(completion), consumer, QPointer<QObject>(consumer)});
    pump();
}
int ImageCache::queuedJobs() const
{
    int count = 0;
    for (const auto &pending : m_pending) if (!pending.active) ++count;
    return count;
}
void ImageCache::cancelRequests(QObject *consumer)
{
    if (!consumer) return;
    const auto keys = m_pending.keys();
    for (const auto &key : keys) {
        auto pending = m_pending.find(key);
        if (pending == m_pending.end()) continue;
        pending->subscribers.removeIf([consumer](const Subscriber &subscriber) { return subscriber.identity == consumer; });
        if (!pending->subscribers.isEmpty()) continue;
        if (pending->active) {
            const auto token = pending->token;
            // An immediate restart can subscribe to the same in-flight frame.
            // Different requests still release orphaned slots on the next tick.
            QTimer::singleShot(0,this,[this,key,token] {
                const auto orphan = m_pending.constFind(key);
                if (orphan == m_pending.cend() || orphan->token != token || !orphan->subscribers.isEmpty()) return;
                const auto reply = orphan->reply;
                if (!reply || reply->isFinished()) return; // Decode is already bounded by the same slot.
                m_pending.remove(key);
                reply->abort();
            });
            continue;
        }
        const auto reply = pending->reply;
        const auto token = pending->token;
        m_pending.erase(pending);
        const auto canceled = [&](const QPair<QString,quint64> &job) { return job.first == key && job.second == token; };
        m_visible.removeIf(canceled);
        m_prefetch.removeIf(canceled);
        if (reply && !reply->isFinished()) reply->abort();
    }
    pump();
}
void ImageCache::pump()
{
    // A slot covers download AND queued/running decompression. Consequently
    // at most four encoded responses (8 MiB each) can await two decoders.
    while (m_activeJobs < 4 && (!m_visible.isEmpty() || !m_prefetch.isEmpty())) {
        const auto priority = m_visible.isEmpty() ? Priority::Prefetch : Priority::Visible;
        const auto job = (priority == Priority::Visible ? m_visible : m_prefetch).dequeue();
        auto pending = m_pending.find(job.first);
        if (pending == m_pending.end() || pending->token != job.second || pending->active || pending->priority != priority) continue;
        pending->active = true;
        ++m_activeJobs;
        startJob(job.first, job.second);
    }
}
void ImageCache::completeJob(const QString &key, quint64 token, const QImage &image, const QString &error)
{
    QList<Subscriber> subscribers;
    const auto pending = m_pending.constFind(key);
    if (pending != m_pending.cend() && pending->token == token) subscribers = m_pending.take(key).subscribers;
    --m_activeJobs;
    for (const auto &subscriber : subscribers)
        if (!subscriber.identity || subscriber.context) subscriber.complete(image,error);
    pump();
}
void ImageCache::startJob(const QString &key, quint64 token)
{
    const QUrl url(key);
    const auto generation = m_generation;
    QNetworkRequest request(url);
    const RequestPolicy policy{20000,25000,8*1024*1024};
    policy.configure(request);
    const auto stored = m_disk ? m_disk->metaData(url) : QNetworkCacheMetaData();
    bool fresh = stored.isValid() && stored.expirationDate() > QDateTime::currentDateTimeUtc();
    for (const auto &header : stored.rawHeaders())
        if (header.first.compare("Cache-Control", Qt::CaseInsensitive) == 0 && !allowsReuse(header.second)) fresh = false;
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
        fresh ? QNetworkRequest::PreferCache : QNetworkRequest::AlwaysNetwork);
    auto *reply = m_network.get(request);
    m_pending[key].reply = reply;
    policy.enforce(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, token, generation] {
        auto pending = m_pending.find(key);
        if (pending != m_pending.end() && pending->token == token) pending->reply = nullptr;
        if (generation != m_generation && m_disk) m_disk->remove(reply->url());
        const auto bytes = reply->isOpen() ? reply->readAll() : QByteArray();
        const auto error = RequestPolicy::deadlineExceeded(reply) ? Meteo::i18n("The request timed out. Try again.")
            : reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
        const auto now = QDateTime::currentDateTimeUtc();
        const auto metadata = m_disk ? m_disk->metaData(reply->url()) : QNetworkCacheMetaData();
        const auto cacheControl = reply->rawHeader("Cache-Control").toLower();
        const bool cacheable = allowsReuse(cacheControl);
        const bool needsExpiry = cacheable && !reply->hasRawHeader("Expires") && !cacheControl.contains("max-age");
        const auto expiry = expirationFor(reply, metadata, now);
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
        if (!error.isEmpty()) { completeJob(key,token,{},error); return; }
        // PNG decompression must not block input or the map's current frame.
        m_decoder.start(QRunnable::create([this, bytes, error, key, token, expiry, cacheable, fallback, generation] {
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
                if (image.isNull()) failure = Meteo::i18n("Could not read the map image.");
            }
            QMetaObject::invokeMethod(this, [this, key, token, image, failure, expiry, cacheable, fallback, bytes, generation] {
                if (!image.isNull() && cacheable && generation == m_generation) {
                    if (m_disk && fallback.isValid()) {
                        if (auto *device = m_disk->prepare(fallback)) {
                            if (device->write(bytes) == bytes.size()) m_disk->insert(device);
                            else delete device;
                        }
                    }
                    m_images.insert(key, new Entry{image, expiry}, int(image.sizeInBytes()));
                }
                completeJob(key,token,image,failure);
            }, Qt::QueuedConnection);
        }));
    });
}
}
