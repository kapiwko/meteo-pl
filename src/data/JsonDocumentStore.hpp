#pragma once
#include "network/DiskCacheBudget.hpp"
#include <QJsonDocument>
#include <QDateTime>
#include <QUrl>
#include <optional>

namespace Meteo {
std::optional<QJsonDocument> readJsonFile(const QString &path,qint64 maximumBytes=5*1024*1024);
bool writeJsonFile(const QString &path,const QJsonDocument &document,DiskCacheBudget budget);
struct CachedDocument { QJsonDocument document; QDateTime savedAt,expiresAt; };
class JsonDocumentStore {
public:
    JsonDocumentStore(QString category,DiskCacheBudget budget):m_category(std::move(category)),m_budget(budget) {}
    QString path(const QUrl &url) const;
    std::optional<CachedDocument> readAny(const QUrl &url) const;
    std::optional<CachedDocument> readFresh(const QUrl &url,int maximumAgeSeconds) const;
    bool write(const QUrl &url,const QJsonDocument &document,const QDateTime &expires={}) const;
    void invalidate(const QUrl &url) const;
private:
    std::optional<CachedDocument> read(const QUrl &url,std::optional<int> maximumAgeSeconds) const;
    QString m_category;
    DiskCacheBudget m_budget;
};
}
