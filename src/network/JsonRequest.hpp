#pragma once
#include "JsonTransport.hpp"
#include "data/JsonDocumentStore.hpp"
namespace Meteo {
class JsonRequest : public QObject {
    Q_OBJECT
public:
    using Validator=JsonTransport::Validator;
    explicit JsonRequest(QObject *parent=nullptr,RequestPolicy policy={});
    void get(const QUrl &url,int cacheSeconds=0,bool force=false,Validator validator={});
    void cancel();
    int statusCode() const { return m_statusCode; }
signals:
    void received(const QJsonDocument &document);
    void failed(const QString &error);
private:
    JsonTransport m_transport;
    JsonDocumentStore m_store{"metadata",metadataCacheBudget};
    QUrl m_url;
    quint64 m_generation=0;
    int m_statusCode=0;
};
}
