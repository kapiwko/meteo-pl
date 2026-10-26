#pragma once
#include "MeteoUiExport.hpp"
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
namespace Meteo { class ImageCache; }

class METEO_UI_EXPORT ImagePreloader : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(int readyCount READ readyCount WRITE setReadyCount NOTIFY changed)
    Q_PROPERTY(int completed READ completed NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit ImagePreloader(QObject *parent = nullptr, Meteo::ImageCache *cache = nullptr);
    bool busy() const { return m_busy; }
    bool ready() const { return m_ready; }
    int readyCount() const { return m_readyCount; }
    void setReadyCount(int count);
    int completed() const { return m_completed; }
    int total() const { return m_urls.size(); }
    QString error() const { return m_error; }
    Q_INVOKABLE void start(const QVariantList &urls);
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void readyChanged();
    void finished();
    void failed();
private:
    void pump(quint64 generation);
    void schedulePump(quint64 generation);
    Meteo::ImageCache *m_cache;
    QList<QUrl> m_urls;
    int m_next = 0, m_completed = 0, m_inFlight = 0, m_readyCount = 0;
    quint64 m_generation = 0;
    bool m_busy = false, m_ready = false, m_pumpScheduled = false;
    QString m_error;
};
