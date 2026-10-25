#pragma once
#include "presentation/MeteoUiExport.hpp"
#include <QCache>
#include <QDateTime>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QThreadPool>
#include <QPointer>
#include <QQueue>
#include <QLockFile>
#include <memory>
#include <functional>
class QNetworkReply;

namespace Meteo {
// One instance per process: engines share requests and decoded images. Different
// applications use separate disk directories. A process lock gives exactly one
// instance ownership of each directory; other instances use memory only.
class METEO_UI_EXPORT ImageCache : public QObject {
public:
    using Completion = std::function<void(const QImage &, const QString &)>;
    using DetailedCompletion = std::function<void(const QImage &, const QString &, int)>;
    enum class Priority { Visible, Prefetch };
    explicit ImageCache(const QString &directory, QObject *parent = nullptr);
    void request(const QUrl &url, Completion completion, QObject *consumer = nullptr, Priority priority = Priority::Visible);
    void requestDetailed(const QUrl &url, DetailedCompletion completion, QObject *consumer = nullptr, Priority priority = Priority::Visible);
    void cancelRequests(QObject *consumer);
    void clearMemory() { m_images.clear(); }
    void clear();
    bool diskCacheEnabled() const { return m_disk != nullptr; }
    int activeJobs() const { return m_activeJobs; }
    int queuedJobs() const;
    static ImageCache *shared();
private:
    static constexpr int maximumPendingJobs = 4096;
    static constexpr int maximumSubscribers = 16384;
    struct Entry { QImage image; QDateTime expires; };
    struct Subscriber { DetailedCompletion complete; QObject *identity; QPointer<QObject> context; };
    struct Pending {
        QList<Subscriber> subscribers;
        QPointer<QNetworkReply> reply;
        quint64 token;
        Priority priority;
        bool active = false;
    };
    void pump();
    void startJob(const QString &key, quint64 token);
    void completeJob(const QString &key, quint64 token, const QImage &image, const QString &error, int status);
    std::unique_ptr<QLockFile> m_diskLock;
    QNetworkAccessManager m_network;
    QNetworkDiskCache *m_disk = nullptr;
    QCache<QString, Entry> m_images{96 * 1024 * 1024};
    QHash<QString, Pending> m_pending;
    QHash<QObject *, QMetaObject::Connection> m_contexts;
    QQueue<QPair<QString,quint64>> m_visible, m_prefetch;
    int m_activeJobs = 0;
    quint64 m_nextToken = 0;
    quint64 m_generation = 0;
    QThreadPool m_decoder;
};
}
