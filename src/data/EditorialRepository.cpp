#include "EditorialRepository.hpp"
namespace Meteo {
EditorialRepository::EditorialRepository(QObject *parent,QUrl endpoint):QObject(parent),m_endpoint(std::move(endpoint))
{
    connect(&m_transport,&JsonTransport::received,this,[this](const QJsonDocument &document,const QDateTime &expires) {
        if (!m_parsed) return;
        if (expires.isValid()) m_store.write(m_url,document,expires); else m_store.invalidate(m_url);
        emit received(*m_parsed);
    });
    connect(&m_transport,&JsonTransport::failed,this,[this](const QString &error) { emit failed(error,m_transport.statusCode(),m_hadCache); });
}
std::optional<EditorialSnapshot> EditorialRepository::decode(const QJsonDocument &document) const
{
    EditorialSnapshot result;
    if (m_commentary) { result.commentary=parseCommentary(document); if (!result.commentary) return {}; }
    else { result.articles=parseArticlePage(document); if (!result.articles) return {}; }
    return result;
}
void EditorialRepository::cancel() { m_transport.cancel(); m_parsed.reset(); }
void EditorialRepository::load(bool commentary,const QString &date,int page,bool force)
{
    cancel(); m_commentary=commentary; m_hadCache=false;
    const auto path=commentary?QStringLiteral("synoptic-comment/")+(date.isEmpty()?QString():date+'/')+"pl":QStringLiteral("articles/pl?page=%1").arg(page);
    m_url=m_endpoint.resolved(QUrl("/api/v1/"+path));
    if (const auto cached=m_store.readAny(m_url)) {
        auto snapshot=decode(cached->document);
        if (snapshot) {
            m_hadCache=true;
            const auto now=QDateTime::currentDateTimeUtc();
            const bool fresh=cached->savedAt.secsTo(now)<1800 && (!cached->expiresAt.isValid() || cached->expiresAt>now);
            snapshot->usingCache=true; snapshot->refreshing=force || !fresh;
            emit received(*snapshot);
            if (!snapshot->refreshing) return;
        }
    }
    m_transport.get(m_url,1800,[this](const QJsonDocument &document) { m_parsed=decode(document); return m_parsed.has_value(); });
}
}
