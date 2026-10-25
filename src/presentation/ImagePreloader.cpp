#include "ImagePreloader.hpp"
#include "network/ImageCache.hpp"
#include <QPointer>
#include <QSet>
#include <QTimer>

ImagePreloader::ImagePreloader(QObject *parent, Meteo::ImageCache *cache)
    : QObject(parent), m_cache(cache ? cache : Meteo::ImageCache::shared()) {}
void ImagePreloader::cancel()
{
    ++m_generation;
    m_busy = m_ready = false;
    m_next = m_completed = m_inFlight = 0;
    m_urls.clear();
    emit changed();
}
void ImagePreloader::start(const QVariantList &urls)
{
    cancel();
    m_error.clear();
    QSet<QUrl> seen;
    for (const auto &value : urls) {
        const QUrl url(value.toString());
        if (url.isEmpty() || seen.contains(url)) continue;
        m_urls.append(url); seen.insert(url);
    }
    m_busy = !m_urls.isEmpty();
    m_ready = !m_busy;
    emit changed();
    if (m_ready) emit finished();
    else pump(m_generation);
}
void ImagePreloader::pump(quint64 generation)
{
    if (generation != m_generation || !m_busy) return;
    // Limit network/decode pressure; cancellation never cancels a request shared
    // with the currently displayed map. The ImageCache itself stays bounded.
    while (m_busy && generation == m_generation && m_inFlight < 4 && m_next < m_urls.size()) {
        const auto url = m_urls[m_next++];
        ++m_inFlight;
        m_cache->request(url, [self=QPointer<ImagePreloader>(this), generation](const QImage &, const QString &error) {
            if (!self || generation != self->m_generation || !self->m_busy) return;
            --self->m_inFlight;
            if (!error.isEmpty()) {
                self->m_busy = false;
                self->m_error = QStringLiteral("Nie udało się przygotować wszystkich klatek animacji. Spróbuj ponownie lub wybierz wcześniejszy start.");
                emit self->changed(); emit self->failed(); return;
            }
            ++self->m_completed;
            if (self->m_completed == self->m_urls.size()) {
                self->m_busy = false; self->m_ready = true;
                emit self->changed(); emit self->finished();
            } else {
                emit self->changed();
                QTimer::singleShot(0, self, [self, generation] { if (self) self->pump(generation); });
            }
        });
    }
}
