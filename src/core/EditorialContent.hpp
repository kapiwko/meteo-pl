#pragma once
#include <QJsonDocument>
#include <QString>
#include <QVector>
#include <optional>

namespace Meteo {
struct Article {
    QString title, html, url, date, image;
};
struct ArticlePage { QVector<Article> articles; };
struct Commentary { QString html, publishedDate; };

[[nodiscard]] std::optional<ArticlePage> parseArticlePage(const QJsonDocument &document);
[[nodiscard]] std::optional<Commentary> parseCommentary(const QJsonDocument &document);
}
