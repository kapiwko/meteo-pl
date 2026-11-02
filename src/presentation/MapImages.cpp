#include "MapImages.hpp"
#include "network/ImageCache.hpp"
#include <QQmlEngine>
#include <QQuickAsyncImageProvider>
#include <QQuickTextureFactory>
#include <QPointer>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <atomic>
#include <memory>

namespace {
struct ResponseState {
    std::atomic_bool canceled=false,finished=false;
};
class Response : public QQuickImageResponse {
public:
    explicit Response(Meteo::ImageCache *cache) : m_cache(cache) {}
    const std::shared_ptr<ResponseState> state=std::make_shared<ResponseState>();
    QImage image;
    QString failure;
    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(image); }
    QString errorString() const override { return failure; }
    void cancel() override {
        state->canceled.store(true);
        if (!m_cache) return;
        QMetaObject::invokeMethod(m_cache, [self=QPointer<Response>(this),cache=m_cache,state=state] {
            if (state->finished.load() || !self || !cache) return;
            cache->cancelRequests(self);
            complete(self,state,{},{});
        }, Qt::QueuedConnection);
    }
    static void complete(const QPointer<Response> &response,const std::shared_ptr<ResponseState> &state,
                         const QImage &result,const QString &error) {
        // Qt may destroy the response on its image-reader thread after finished.
        // Keep the once-only completion gate alive for already queued callbacks.
        if (state->finished.exchange(true) || !response) return;
        response->image=result; response->failure=error; emit response->finished();
    }
private:
    QPointer<Meteo::ImageCache> m_cache;
};
class Provider : public QQuickAsyncImageProvider {
public:
    explicit Provider(Meteo::ImageCache *cache,MapImages *events) : m_cache(cache),m_events(events) {}
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &size) override
    {
        auto *response = new Response(m_cache);
        const auto url = QUrl::fromEncoded(QByteArray::fromBase64(id.toLatin1(), QByteArray::Base64UrlEncoding));
        QMetaObject::invokeMethod(m_cache, [cache = m_cache, events=m_events, response = QPointer<Response>(response), state=response->state, url, size] {
            if (!response) return;
            if (state->canceled.load()) { Response::complete(response,state,{},{}); return; }
            cache->requestDetailed(url, [response, state, events, url, size](const QImage &image, const QString &error,int status) {
                if (state->finished.load() || !response) return;
                if (!error.isEmpty() && events) emit events->imageFailed(url,error,status);
                Response::complete(response,state,size.isValid() ? image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation) : image,error);
            }, response);
        }, Qt::QueuedConnection);
        return response;
    }
private:
    Meteo::ImageCache *m_cache;
    QPointer<MapImages> m_events;
};
}
MapImages *MapImages::create(QQmlEngine *engine, QJSEngine *)
{
    auto *events=new MapImages;
    engine->addImageProvider(QStringLiteral("meteo-map"), new Provider(Meteo::ImageCache::shared(),events));
    return events;
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
