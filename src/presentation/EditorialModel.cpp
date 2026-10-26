#include "core/Localization.hpp"
#include "EditorialModel.hpp"
#include "core/EditorialContent.hpp"
#include "network/DiskCacheBudget.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QDate>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <utility>

EditorialModel::EditorialModel(QObject *parent, QUrl endpoint, Clock clock)
    : QObject(parent), m_endpoint(std::move(endpoint)),
      m_clock(clock ? std::move(clock) : Clock([] { return QDate::currentDate(); })), m_today(m_clock())
{
    m_calendarTimer.setInterval(60000);
    connect(&m_calendarTimer,&QTimer::timeout,this,&EditorialModel::updateCalendarDay);
    m_calendarTimer.start();
    connect(&m_request, &Meteo::JsonRequest::received, this, [this](const QJsonDocument &document) {
        m_busy = false;
        if (applyDocument(document)) {
            m_cached = false;
            saveCache(document);
        } else m_error = Meteo::i18n("Invalid meteo.pl response format.");
        emit changed();
    });
    connect(&m_request, &Meteo::JsonRequest::failed, this, [this](const QString &error) {
        m_busy = false;
        if (m_cached) {
            m_notice = Meteo::i18n("Could not refresh. Showing content from the cache.");
        } else if (m_commentary && m_request.statusCode() == 404) {
            m_notice = m_date.isEmpty() ? Meteo::i18n("The synoptic commentary has not been published yet. Try again later.")
                : Meteo::i18n("No synoptic commentary for %1. Choose another date or the latest commentary.").arg(m_date);
        } else m_error = error;
        emit changed();
    });
}
void EditorialModel::setCommentary(bool value) { if (m_commentary == value) return; m_commentary = value; m_request.cancel(); m_busy = false; m_error.clear(); m_notice.clear(); m_articles.clear(); m_pages.clear(); m_page = 0; m_commentHtml.clear(); m_publishedDate.clear(); m_cached = false; emit changed(); }
void EditorialModel::setDate(const QString &value)
{
    if (m_date == value) return;
    if (!value.isEmpty()) {
        const auto date = QDate::fromString(value, Qt::ISODate);
        if (!date.isValid()) { m_notice = Meteo::i18n("Enter a valid date in yyyy-mm-dd format."); emit changed(); return; }
        if (date > m_clock()) { m_notice = Meteo::i18n("Commentary for future dates is not available yet."); emit changed(); return; }
    }
    m_date = value; m_commentHtml.clear(); m_publishedDate.clear(); reload(false);
}
void EditorialModel::updateCalendarDay()
{
    const auto today=m_clock();
    if (!today.isValid() || today==m_today) return;
    m_today=today;
    emit changed();
}
void EditorialModel::reload(bool force) { load(1, force); }
void EditorialModel::more() { if (!m_busy && !m_commentary) load(m_page + 1); }
void EditorialModel::load(int page, bool force)
{
    m_request.cancel();
    m_requestedPage = page; m_busy = true; m_error.clear(); m_notice.clear();
    const auto path = m_commentary ? QStringLiteral("synoptic-comment/") + (m_date.isEmpty() ? QString() : m_date + '/') + QStringLiteral("pl")
                                  : QStringLiteral("articles/pl?page=%1").arg(page);
    m_url = m_endpoint.resolved(QUrl(QStringLiteral("/api/v1/") + path));
    m_cached = loadCache();
    if (!force && m_cached && m_savedAt.secsTo(QDateTime::currentDateTimeUtc()) < 1800) {
        m_busy = false;
        emit changed();
        return;
    }
    emit changed();
    m_request.get(m_url);
}
bool EditorialModel::applyDocument(const QJsonDocument &document)
{
    if (m_commentary) {
        const auto comment = Meteo::parseCommentary(document);
        if (!comment) return false;
        m_commentHtml = comment->html;
        m_publishedDate = comment->publishedDate;
        return true;
    }
    const auto page = Meteo::parseArticlePage(document);
    if (!page) return false;
    QVariantList entries;
    for (const auto &article : page->articles) {
        entries.append(QVariantMap{{"title", article.title}, {"html", article.html},
            {"url", article.url}, {"date", article.date}, {"image", article.image}});
    }
    m_page = m_requestedPage;
    if (m_page == 1) m_pages.clear();
    m_pages[m_page] = entries;
    m_articles.clear();
    for (const auto &entries : std::as_const(m_pages)) m_articles.append(entries);
    return true;
}
QString EditorialModel::cachePath() const
{
    const auto hash = QCryptographicHash::hash(m_url.toEncoded(), QCryptographicHash::Sha256).toHex();
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/meteo-pl/editorial/") + QString::fromLatin1(hash) + QStringLiteral(".json");
}
bool EditorialModel::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 5 * 1024 * 1024) return false;
    const auto envelope = QJsonDocument::fromJson(file.readAll()).object();
    const auto savedAt = QDateTime::fromString(envelope.value("savedAt").toString(), Qt::ISODate);
    const auto value = envelope.value("document");
    if (envelope.value("url").toString() != m_url.toString(QUrl::FullyEncoded)
        || !savedAt.isValid() || savedAt > QDateTime::currentDateTimeUtc()
        || (m_commentary ? !value.isObject() || !value.toObject().value("content").isString() : !value.isArray())) return false;
    if (!applyDocument(m_commentary ? QJsonDocument(value.toObject()) : QJsonDocument(value.toArray()))) return false;
    m_savedAt = savedAt;
    Meteo::touchCacheEntry(file.fileName());
    return true;
}
void EditorialModel::saveCache(const QJsonDocument &document) const
{
    if (m_commentary ? !document.isObject() || !document.object().value("content").isString() : !document.isArray()) return;
    const auto path = cachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
    const QJsonObject envelope{{"url", m_url.toString(QUrl::FullyEncoded)},
        {"savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"document", m_commentary ? QJsonValue(document.object()) : QJsonValue(document.array())}};
    file.write(QJsonDocument(envelope).toJson(QJsonDocument::Compact));
    if (file.commit()) Meteo::pruneDiskCache(QFileInfo(path).absolutePath(),{QStringLiteral("*.json")},Meteo::editorialCacheBudget,path);
}
