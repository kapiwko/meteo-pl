#include "core/Localization.hpp"
#include "EditorialModel.hpp"
#include "core/EditorialContent.hpp"
#include <QDate>
#include <utility>

EditorialModel::EditorialModel(QObject *parent, QUrl endpoint, Clock clock)
    : QObject(parent), m_repository(nullptr,std::move(endpoint)),
      m_clock(clock ? std::move(clock) : Clock([] { return QDate::currentDate(); })), m_today(m_clock())
{
    m_calendarTimer.setInterval(60000);
    connect(&m_calendarTimer,&QTimer::timeout,this,&EditorialModel::updateCalendarDay);
    m_calendarTimer.start();
    connect(&m_repository,&Meteo::EditorialRepository::received,this,[this](const Meteo::EditorialSnapshot &snapshot) {
        m_busy=snapshot.refreshing; m_cached=snapshot.usingCache;
        if (snapshot.commentary) { m_comment.html=snapshot.commentary->html; m_comment.publishedDate=snapshot.commentary->publishedDate; }
        if (snapshot.articles) m_article.accept(*snapshot.articles);
        emit changed();
    });
    connect(&m_repository,&Meteo::EditorialRepository::failed,this,[this](const QString &error,int status,bool cached) {
        m_busy=false;
        if (cached) { m_notice=Meteo::i18n("Could not refresh. Showing content from the cache."); if (status==200) m_error=error; }
        else if (m_commentary && status==404) m_notice=m_comment.date.isEmpty()
            ? Meteo::i18n("The synoptic commentary has not been published yet. Try again later.")
            : Meteo::i18n("No synoptic commentary for %1. Choose another date or the latest commentary.").arg(m_comment.date);
        else m_error=error;
        emit changed();
    });
}

void EditorialModel::setCommentary(bool value) { if (m_commentary == value) return; m_commentary = value; m_repository.cancel(); m_busy = false; m_error.clear(); m_notice.clear(); m_article.entries.clear(); m_article.pages.clear(); m_article.page = 0; m_comment.html.clear(); m_comment.publishedDate.clear(); m_cached = false; emit changed(); }
void EditorialModel::setDate(const QString &value)
{
    if (m_comment.date == value) return;
    if (!value.isEmpty()) {
        const auto date = QDate::fromString(value, Qt::ISODate);
        if (!date.isValid()) { m_notice = Meteo::i18n("Enter a valid date in yyyy-mm-dd format."); emit changed(); return; }
        if (date > m_clock()) { m_notice = Meteo::i18n("Commentary for future dates is not available yet."); emit changed(); return; }
    }
    m_comment.date = value; m_comment.html.clear(); m_comment.publishedDate.clear(); reload(false);
}
void EditorialModel::updateCalendarDay()
{
    const auto today=m_clock();
    if (!today.isValid() || today==m_today) return;
    m_today=today;
    emit changed();
}
void EditorialModel::reload(bool force) { load(1, force); }
void EditorialModel::more() { if (!m_busy && !m_commentary) load(m_article.page + 1); }
void EditorialModel::load(int page, bool force)
{
    m_repository.cancel();
    m_article.requestedPage = page; m_busy = true; m_error.clear(); m_notice.clear();
    m_cached=false;
    emit changed();
    m_repository.load(m_commentary,m_comment.date,page,force);
}
