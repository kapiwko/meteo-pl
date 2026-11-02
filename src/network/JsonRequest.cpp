#include "JsonRequest.hpp"
#include <QTimer>
namespace Meteo {
JsonRequest::JsonRequest(QObject *parent,RequestPolicy policy):QObject(parent),m_transport(nullptr,policy)
{
    connect(&m_transport,&JsonTransport::received,this,[this](const QJsonDocument &document,const QDateTime &expires) {
        m_statusCode=200;
        if (expires.isValid()) m_store.write(m_url,document,expires); else m_store.invalidate(m_url);
        emit received(document);
    });
    connect(&m_transport,&JsonTransport::failed,this,[this](const QString &error) { m_statusCode=m_transport.statusCode(); emit failed(error); });
}
void JsonRequest::cancel() { ++m_generation; m_transport.cancel(); }
void JsonRequest::get(const QUrl &url,int lifetime,bool force,Validator validator)
{
    cancel(); m_statusCode=0; m_url=url;
    if (lifetime>0 && !force) {
        const auto cached=m_store.readFresh(url,lifetime);
        if (cached && (!validator || validator(cached->document))) {
            const auto generation=m_generation; const auto document=cached->document;
            QTimer::singleShot(0,this,[this,generation,document] { if (generation==m_generation) { m_statusCode=200; emit received(document); } });
            return;
        }
    }
    m_transport.get(url,lifetime,std::move(validator));
}
}
