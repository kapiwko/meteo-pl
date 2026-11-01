#pragma once
#include "network/JsonRequest.hpp"
#include <QVariantList>
namespace Meteo {
class GeocodingClient : public QObject {
    Q_OBJECT
public:
    explicit GeocodingClient(QObject *parent=nullptr,QUrl endpoint=QUrl("https://photon.dev.meteo.pl/api/"));
    void search(const QString &query,bool polandOnly);
    void cancel() { m_request.cancel(); }
signals:
    void received(const QVariantList &places);
    void failed(const QString &error);
private:
    JsonRequest m_request;
    QUrl m_endpoint;
};
}
