#pragma once
#include <QCache>
#include <QDateTime>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QThreadPool>
#include <functional>

namespace Meteo {
// One instance per process: engines share requests and decoded images. Different
// applications use separate disk directories, as required by QNetworkDiskCache.
class ImageCache : public QObject {
public:
    using Completion = std::function<void(const QImage &, const QString &)>;
    explicit ImageCache(const QString &directory, QObject *parent = nullptr);
    void request(const QUrl &url, Completion completion);
    void clearMemory() { m_images.clear(); }
    static ImageCache *shared();
private:
    struct Entry { QImage image; QDateTime expires; };
    QNetworkAccessManager m_network;
    QNetworkDiskCache *m_disk;
    QCache<QString, Entry> m_images{96 * 1024 * 1024};
    QHash<QString, QList<Completion>> m_pending;
    QThreadPool m_decoder;
};
}
