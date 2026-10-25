#pragma once
#include "presentation/MeteoUiExport.hpp"
#include <QCache>
#include <QDateTime>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QThreadPool>
#include <QLockFile>
#include <memory>
#include <functional>

namespace Meteo {
// One instance per process: engines share requests and decoded images. Different
// applications use separate disk directories. A process lock gives exactly one
// instance ownership of each directory; other instances use memory only.
class METEO_UI_EXPORT ImageCache : public QObject {
public:
    using Completion = std::function<void(const QImage &, const QString &)>;
    explicit ImageCache(const QString &directory, QObject *parent = nullptr);
    void request(const QUrl &url, Completion completion);
    void clearMemory() { m_images.clear(); }
    void clear();
    bool diskCacheEnabled() const { return m_disk != nullptr; }
    static ImageCache *shared();
private:
    struct Entry { QImage image; QDateTime expires; };
    std::unique_ptr<QLockFile> m_diskLock;
    QNetworkAccessManager m_network;
    QNetworkDiskCache *m_disk = nullptr;
    QCache<QString, Entry> m_images{96 * 1024 * 1024};
    QHash<QString, QList<Completion>> m_pending;
    quint64 m_generation = 0;
    QThreadPool m_decoder;
};
}
