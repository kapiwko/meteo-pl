#include "EditorialContent.hpp"
#include <QJsonArray>
#include <QJsonObject>

namespace Meteo {
std::optional<ArticlePage> parseArticlePage(const QJsonDocument &document)
{
    if (!document.isArray() || document.array().size() > 1000) return std::nullopt;
    ArticlePage page;
    for (const auto &value : document.array()) {
        if (!value.isObject()) return std::nullopt;
        const auto object = value.toObject();
        if (!object.value("title").isString() || !object.value("content").isString()) return std::nullopt;
        page.articles.append({object.value("title").toString(), object.value("content").toString(),
            object.value("link").toString(), object.value("date_gmt").toString().left(10),
            object.value("featured_media_urls").toObject().value("medium").toString()});
    }
    return page;
}
std::optional<Commentary> parseCommentary(const QJsonDocument &document)
{
    if (!document.isObject() || !document.object().value("content").isString()) return std::nullopt;
    return Commentary{document.object().value("content").toString(), document.object().value("published_date").toString()};
}
}
