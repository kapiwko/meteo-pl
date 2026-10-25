#include "core/Localization.hpp"
#include "JsonRequest.hpp"
#include "DiskCacheBudget.hpp"
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QDateTime>

namespace Meteo {
namespace {
QString cachePath(const QUrl &url)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/meteo-pl/metadata/")
        + QString::fromLatin1(QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex())
        + QStringLiteral(".json");
}
QJsonDocument readCache(const QUrl &url, int seconds)
{
    QFile file(cachePath(url));
    if (!file.open(QIODevice::ReadOnly) || file.size()>5*1024*1024) return {};
    const auto entry=QJsonDocument::fromJson(file.readAll()).object();
    const auto saved=QDateTime::fromString(entry.value("savedAt").toString(),Qt::ISODate);
    const auto age=saved.secsTo(QDateTime::currentDateTimeUtc());
    if (entry.value("url").toString()!=url.toString(QUrl::FullyEncoded) || !saved.isValid() || age<0 || age>=seconds) return {};
    const auto value=entry.value("document");
    const auto document = value.isObject()?QJsonDocument(value.toObject()):value.isArray()?QJsonDocument(value.toArray()):QJsonDocument();
    if (!document.isNull()) touchCacheEntry(file.fileName());
    return document;
}
void writeCache(const QUrl &url,const QJsonDocument &document)
{
    const auto path=cachePath(url);
    QDir directory(QFileInfo(path).absolutePath());
    if (!directory.mkpath(QStringLiteral("."))) return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
    const QJsonObject entry{{"url",url.toString(QUrl::FullyEncoded)},
        {"savedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"document",document.isObject()?QJsonValue(document.object()):QJsonValue(document.array())}};
    file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact));
    if (!file.commit()) return;
    pruneDiskCache(directory.absolutePath(),{QStringLiteral("*.json")},metadataCacheBudget,path);
}
}
JsonRequest::JsonRequest(QObject *parent, RequestPolicy policy) : QObject(parent), m_policy(policy) {}
void JsonRequest::cancel()
{
    ++m_generation;
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}
void JsonRequest::get(const QUrl &url, int cacheSeconds, bool force, Validator validator)
{
    cancel();
    m_statusCode = 0;
    if (cacheSeconds>0 && !force) {
        const auto document=readCache(url,cacheSeconds);
        if (!document.isNull() && (!validator || validator(document))) {
            const auto generation=m_generation;
            QTimer::singleShot(0,this,[this,generation,document] {
                if (generation!=m_generation) return;
                m_statusCode=200;
                emit received(document);
            });
            return;
        }
    }
    QNetworkRequest request(url);
    m_policy.configure(request);
    request.setRawHeader("User-Agent", "MeteoDesktop/0.2 (Qt desktop client)");
    request.setRawHeader("Accept", "application/json");
    auto *reply = m_network.get(request);
    m_reply = reply;
    const auto generation = m_generation;
    m_policy.enforce(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, url, cacheSeconds, validator=std::move(validator)] {
        reply->deleteLater();
        if (generation != m_generation) return;
        m_reply = nullptr;
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_statusCode = status;
        if (RequestPolicy::deadlineExceeded(reply)) {
            emit failed(Meteo::i18n("The request timed out. Try again."));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            emit failed(Meteo::i18n("Could not download data (HTTP %1). %2").arg(status).arg(reply->errorString()));
            return;
        }
        QJsonParseError error;
        const auto body = reply->readAll();
        const auto document = body.trimmed() == "null" ? QJsonDocument(QJsonArray{}) : QJsonDocument::fromJson(body, &error);
        if (error.error != QJsonParseError::NoError || document.isNull()) {
            emit failed(Meteo::i18n("The server returned an invalid JSON document."));
            return;
        }
        if (validator && !validator(document)) {
            emit failed(Meteo::i18n("Invalid meteo.pl response format."));
            return;
        }
        const auto policy=reply->rawHeader("Cache-Control").toLower();
        if (cacheSeconds>0 && !policy.contains("no-store") && !policy.contains("no-cache")) writeCache(url,document);
        emit received(document);
    });
}
}
