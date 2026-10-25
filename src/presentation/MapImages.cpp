#include "MapImages.hpp"
#include "network/ImageCache.hpp"
#include <QQmlEngine>
#include <QQuickAsyncImageProvider>
#include <QQuickTextureFactory>
#include <QPointer>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <atomic>

namespace {
class Response : public QQuickImageResponse {
public:
    explicit Response(Meteo::ImageCache *cache) : m_cache(cache) {}
    QImage image;
    QString failure;
    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(image); }
    QString errorString() const override { return failure; }
    bool canceled() const { return m_canceled.load(); }
    void cancel() override {
        m_canceled.store(true);
        if (!m_cache) return;
        QMetaObject::invokeMethod(m_cache, [self=QPointer<Response>(this),cache=m_cache] {
            if (!self || !cache) return;
            cache->cancelRequests(self);
            self->complete({},{});
        }, Qt::QueuedConnection);
    }
    void complete(const QImage &result, const QString &error) {
        if (m_finished.test_and_set()) return;
        image = result; failure = error; emit finished();
    }
private:
    QPointer<Meteo::ImageCache> m_cache;
    std::atomic_bool m_canceled = false;
    std::atomic_flag m_finished = ATOMIC_FLAG_INIT;
};
class Provider : public QQuickAsyncImageProvider {
public:
    explicit Provider(Meteo::ImageCache *cache) : m_cache(cache) {}
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &size) override
    {
        auto *response = new Response(m_cache);
        const auto url = QUrl::fromEncoded(QByteArray::fromBase64(id.toLatin1(), QByteArray::Base64UrlEncoding));
        QMetaObject::invokeMethod(m_cache, [cache = m_cache, response = QPointer<Response>(response), url, size] {
            if (!response) return;
            if (response->canceled()) { response->complete({},{}); return; }
            cache->request(url, [response, size](const QImage &image, const QString &error) {
                if (response) response->complete(size.isValid() ? image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation) : image, error);
            }, response);
        }, Qt::QueuedConnection);
        return response;
    }
private:
    Meteo::ImageCache *m_cache;
};
}
MapImages *MapImages::create(QQmlEngine *engine, QJSEngine *)
{
    engine->addImageProvider(QStringLiteral("meteo-map"), new Provider(Meteo::ImageCache::shared()));
    return new MapImages;
}
QUrl MapImages::cached(const QUrl &url) const
{
    if (url.isEmpty()) return {};
    return QUrl(QStringLiteral("image://meteo-map/") + QString::fromLatin1(url.toEncoded().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)));
}
void MapImages::prefetch(const QUrl &url) const
{
    if (!url.isEmpty()) Meteo::ImageCache::shared()->request(url, [](const QImage &, const QString &) {}, nullptr, Meteo::ImageCache::Priority::Prefetch);
}
QString MapImages::cachedHtml(const QString &html, const QUrl &base) const
{
    // Keep the original markup and dimensions; change only remote image URLs.
    static const QRegularExpression images(QStringLiteral(R"re(<img\b[^>]*?\bsrc\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))re"),
        QRegularExpression::CaseInsensitiveOption);
    auto matches = images.globalMatch(html);
    QString result;
    qsizetype position = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        int group = 1;
        while (group < 3 && match.capturedStart(group) < 0) ++group;
        if (match.captured(group).isEmpty()) continue;
        const auto source = QTextDocumentFragment::fromHtml(match.captured(group)).toPlainText();
        const auto url = base.resolved(QUrl(source));
        if (url.scheme() != "https" && url.scheme() != "http") continue;
        const auto start = match.capturedStart(group);
        result += html.sliced(position, start - position) + cached(url).toString();
        position = start + match.capturedLength(group);
    }
    return result + html.sliced(position);
}
