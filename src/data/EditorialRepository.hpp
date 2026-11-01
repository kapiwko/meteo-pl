#pragma once
#include "core/EditorialContent.hpp"
#include "network/JsonTransport.hpp"
#include "JsonDocumentStore.hpp"
namespace Meteo {
struct EditorialSnapshot {
    std::optional<Commentary> commentary;
    std::optional<ArticlePage> articles;
    bool usingCache=false,refreshing=false;
};
class EditorialRepository : public QObject {
    Q_OBJECT
public:
    explicit EditorialRepository(QObject *parent=nullptr,QUrl endpoint=QUrl("https://meteo-backend.dev.meteo.pl"));
    void load(bool commentary,const QString &date,int page,bool force);
    void cancel();
signals:
    void received(const Meteo::EditorialSnapshot &snapshot);
    void failed(const QString &error,int status,bool hadCache);
private:
    std::optional<EditorialSnapshot> decode(const QJsonDocument &document) const;
    JsonTransport m_transport;
    JsonDocumentStore m_store{"editorial",editorialCacheBudget};
    QUrl m_endpoint,m_url;
    bool m_commentary=true,m_hadCache=false;
    std::optional<EditorialSnapshot> m_parsed;
};
}
Q_DECLARE_METATYPE(Meteo::EditorialSnapshot)
