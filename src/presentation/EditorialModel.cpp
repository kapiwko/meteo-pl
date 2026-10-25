#include "EditorialModel.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QDate>

EditorialModel::EditorialModel(QObject *parent, QUrl endpoint) : QObject(parent), m_endpoint(std::move(endpoint))
{
    connect(&m_request, &Meteo::JsonRequest::received, this, [this](const QJsonDocument &document) {
        m_busy = false;
        if (m_commentary) {
            m_commentHtml = document.object().value("content").toString();
            m_publishedDate = document.object().value("published_date").toString();
        } else {
            if (m_page == 1) m_articles.clear();
            for (const auto &entry : document.array()) {
                const auto object = entry.toObject();
                m_articles.append(QVariantMap{{"title", object.value("title").toString()}, {"html", object.value("content").toString()},
                                              {"url", object.value("link").toString()}, {"date", object.value("date_gmt").toString().left(10)},
                                              {"image", object.value("featured_media_urls").toObject().value("medium").toString()}});
            }
        }
        emit changed();
    });
    connect(&m_request, &Meteo::JsonRequest::failed, this, [this](const QString &error) {
        m_busy = false;
        if (m_commentary && m_request.statusCode() == 404) {
            m_notice = m_date.isEmpty() ? QStringLiteral("Komentarz synoptyka nie został jeszcze opublikowany. Spróbuj później.")
                : QStringLiteral("Brak komentarza synoptyka z %1. Wybierz inny dzień lub najnowszy komentarz.").arg(m_date);
        } else m_error = error;
        emit changed();
    });
}
void EditorialModel::setCommentary(bool value) { if (m_commentary == value) return; m_commentary = value; m_request.cancel(); m_busy = false; m_error.clear(); m_notice.clear(); m_articles.clear(); m_commentHtml.clear(); emit changed(); }
void EditorialModel::setDate(const QString &value)
{
    if (m_date == value) return;
    if (!value.isEmpty()) {
        const auto date = QDate::fromString(value, Qt::ISODate);
        if (!date.isValid()) { m_notice = QStringLiteral("Podaj poprawną datę w formacie rrrr-mm-dd."); emit changed(); return; }
        if (date > QDate::currentDate()) { m_notice = QStringLiteral("Komentarze z przyszłych dni nie są jeszcze dostępne."); emit changed(); return; }
    }
    m_date = value; reload();
}
void EditorialModel::reload() { load(1); }
void EditorialModel::more() { if (!m_busy && !m_commentary) load(m_page + 1); }
void EditorialModel::load(int page)
{
    m_page = page; m_busy = true; m_error.clear(); m_notice.clear();
    if (m_commentary) m_commentHtml.clear();
    emit changed();
    const auto path = m_commentary ? QStringLiteral("synoptic-comment/") + (m_date.isEmpty() ? QString() : m_date + '/') + QStringLiteral("pl")
                                  : QStringLiteral("articles/pl?page=%1").arg(page);
    m_request.get(m_endpoint.resolved(QUrl(QStringLiteral("/api/v1/") + path)));
}
