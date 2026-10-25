#pragma once
#include "MeteoUiExport.hpp"
#include "network/JsonRequest.hpp"
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT PlaceSearchModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool polandOnly READ polandOnly WRITE setPolandOnly NOTIFY queryChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY stateChanged)
public:
    explicit PlaceSearchModel(QObject *parent = nullptr, QUrl endpoint = QUrl(QStringLiteral("https://nom-api.dev.meteo.pl/api/geocode")),
                              QUrl globalEndpoint = QUrl(QStringLiteral("https://photon.dev.meteo.pl/api/")));
    QString query() const { return m_query; }
    bool polandOnly() const { return m_polandOnly; }
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    QVariantList results() const { return m_results; }
    void setQuery(const QString &value);
    void setPolandOnly(bool value);
    Q_INVOKABLE void search();
private:
    void schedule();
    Meteo::JsonRequest m_request;
    QTimer m_delay;
    QString m_query, m_error;
    QVariantList m_results;
    bool m_polandOnly = true, m_busy = false;
    QUrl m_endpoint;
    QUrl m_globalEndpoint;
signals:
    void queryChanged();
    void stateChanged();
};
