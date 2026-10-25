#pragma once
#include "MeteoUiExport.hpp"
#include <QObject>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT CacheStorage : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(qint64 size READ size NOTIFY changed)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY changed)
    Q_PROPERTY(QString forecastSize READ forecastSize NOTIFY changed)
    Q_PROPERTY(QString editorialSize READ editorialSize NOTIFY changed)
    Q_PROPERTY(QString imageSize READ imageSize NOTIFY changed)
    Q_PROPERTY(QString lookupSize READ lookupSize NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit CacheStorage(QObject *parent = nullptr);
    qint64 size() const { return m_size; }
    QString sizeText() const;
    QString forecastSize() const;
    QString editorialSize() const;
    QString imageSize() const;
    QString lookupSize() const;
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clear();
    static QString directory();
signals:
    void changed();
    void cleared();
private:
    void inspect(bool remove);
    qint64 m_size = 0, m_forecasts = 0, m_editorial = 0, m_images = 0, m_lookups = 0;
    bool m_busy = false;
    QString m_error;
    QThreadPool m_worker;
};
