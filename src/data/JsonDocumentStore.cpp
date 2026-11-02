#include "JsonDocumentStore.hpp"
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QJsonObject>
#include <QJsonArray>

namespace Meteo {
std::optional<QJsonDocument> readJsonFile(const QString &path,qint64 maximumBytes)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly) || file.size()>maximumBytes) return {};
    QJsonParseError error; const auto document=QJsonDocument::fromJson(file.readAll(),&error);
    return error.error==QJsonParseError::NoError && !document.isNull()?std::optional(document):std::nullopt;
}
bool writeJsonFile(const QString &path,const QJsonDocument &document,DiskCacheBudget budget)
{
    const auto directory=QFileInfo(path).absolutePath(); if (!QDir().mkpath(directory)) return false;
    QSaveFile file(path); const auto bytes=document.toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) return false;
    pruneDiskCache(directory,{"*.json"},budget,path); return true;
}
QString JsonDocumentStore::path(const QUrl &url) const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)+"/meteo-pl/"+m_category+'/'
        +QString::fromLatin1(QCryptographicHash::hash(url.toEncoded(),QCryptographicHash::Sha256).toHex())+".json";
}
std::optional<CachedDocument> JsonDocumentStore::readAny(const QUrl &url) const { return read(url,std::nullopt); }
std::optional<CachedDocument> JsonDocumentStore::readFresh(const QUrl &url,int maximumAgeSeconds) const
{ return maximumAgeSeconds>0 ? read(url,maximumAgeSeconds) : std::nullopt; }
std::optional<CachedDocument> JsonDocumentStore::read(const QUrl &url,std::optional<int> maximumAgeSeconds) const
{
    const auto document=readJsonFile(path(url)); if (!document || !document->isObject()) return {};
    const auto entry=document->object(); const auto now=QDateTime::currentDateTimeUtc();
    const auto saved=QDateTime::fromString(entry["savedAt"].toString(),Qt::ISODate);
    const auto expires=QDateTime::fromString(entry["expiresAt"].toString(),Qt::ISODate);
    if (entry["url"].toString()!=url.toString(QUrl::FullyEncoded) || !saved.isValid() || saved>now) return {};
    if (maximumAgeSeconds && (saved.secsTo(now)>=*maximumAgeSeconds || (expires.isValid() && expires<=now))) return {};
    const auto value=entry["document"];
    if (!value.isObject() && !value.isArray()) return {};
    touchCacheEntry(path(url));
    return CachedDocument{value.isObject()?QJsonDocument(value.toObject()):QJsonDocument(value.toArray()),saved,expires};
}
bool JsonDocumentStore::write(const QUrl &url,const QJsonDocument &document,const QDateTime &expires) const
{
    if (document.isNull()) return false;
    QJsonObject entry{{"url",url.toString(QUrl::FullyEncoded)},{"savedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"document",document.isObject()?QJsonValue(document.object()):QJsonValue(document.array())}};
    if (expires.isValid()) entry["expiresAt"]=expires.toString(Qt::ISODate);
    return writeJsonFile(path(url),QJsonDocument(entry),m_budget);
}
void JsonDocumentStore::invalidate(const QUrl &url) const { QFile::remove(path(url)); }
}
