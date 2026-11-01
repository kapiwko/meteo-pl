#pragma once
#include "core/EditorialContent.hpp"
#include <QVariantList>
#include <QMap>

struct ArticleNavigation {
    int page=0,requestedPage=1;
    QVariantList entries;
    QMap<int,QVariantList> pages;
    void accept(const Meteo::ArticlePage &document) {
        QVariantList next;
        for (const auto &article : document.articles) next.append(QVariantMap{{"title",article.title},{"html",article.html},
            {"url",article.url},{"date",article.date},{"image",article.image}});
        page=requestedPage; if (page==1) pages.clear(); pages[page]=next;
        entries.clear(); for (const auto &values : std::as_const(pages)) entries.append(values);
    }
};
struct CommentaryNavigation { QString date,html,publishedDate; };
