#include "core/Localization.hpp"
#include "ImagePreloader.hpp"
#include "network/ImageCache.hpp"
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <algorithm>

ImagePreloader::ImagePreloader(QObject *parent, Meteo::ImageCache *cache)
    : QObject(parent), m_cache(cache ? cache : Meteo::ImageCache::shared()) {}
void ImagePreloader::setReadyCount(int count)
{
    count = std::clamp(count, 0, 4096);
    if (m_readyCount == count) return;
    m_readyCount = count; emit changed();
}
void ImagePreloader::cancel()
{
    ++m_generation;
    const auto wasReady = m_ready;
    m_busy = m_ready = m_pumpScheduled = false;
    m_next = m_completed = m_inFlight = 0;
    m_urls.clear();
    m_cache->cancelRequests(this);
    if (wasReady) emit readyChanged();
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
    if (m_ready) { emit readyChanged(); emit finished(); }
    else pump(m_generation);
}
void ImagePreloader::pump(quint64 generation)
{
    if (generation != m_generation || !m_busy) return;
    // The cache also limits complete download/decode jobs across all consumers.
    while (m_busy && generation == m_generation && m_inFlight < 4 && m_next < m_urls.size()) {
        const auto url = m_urls[m_next++];
        ++m_inFlight;
        m_cache->request(url, [self=QPointer<ImagePreloader>(this), generation](const QImage &, const QString &error) {
            if (!self || generation != self->m_generation || !self->m_busy) return;
            --self->m_inFlight;
            if (!error.isEmpty()) {
                self->m_busy = false;
                const auto wasReady = self->m_ready; self->m_ready = false;
                if (wasReady) emit self->readyChanged();
                self->m_error = Meteo::i18n("Could not prepare all animation frames. Try again or choose an earlier run.");
                self->m_cache->cancelRequests(self);
                emit self->changed(); emit self->failed(); return;
            }
            ++self->m_completed;
            const bool ready = self->m_completed >= (self->m_readyCount > 0
                ? std::min(self->m_readyCount, int(self->m_urls.size())) : int(self->m_urls.size()));
            if (ready != self->m_ready) { self->m_ready = ready; emit self->readyChanged(); }
            if (self->m_completed == self->m_urls.size()) {
                self->m_busy = false; self->m_ready = true;
                emit self->changed(); emit self->finished();
            } else {
                emit self->changed();
                self->schedulePump(generation);
            }
        }, this, Meteo::ImageCache::Priority::Prefetch);
    }
    if (m_busy && m_inFlight < 4 && m_next < m_urls.size()) schedulePump(generation);
}
void ImagePreloader::schedulePump(quint64 generation)
{
    if (generation != m_generation || m_pumpScheduled) return;
    m_pumpScheduled = true;
    QTimer::singleShot(0, this, [this, generation] {
        if (generation != m_generation) return;
        m_pumpScheduled = false;
        pump(generation);
    });
}
