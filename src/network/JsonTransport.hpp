#pragma once
#include "RequestPolicy.hpp"
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QDateTime>
#include <functional>
namespace Meteo {
class JsonTransport : public QObject {
    Q_OBJECT
public:
    using Validator=std::function<bool(const QJsonDocument &)>;
    explicit JsonTransport(QObject *parent=nullptr,RequestPolicy policy={}):QObject(parent),m_policy(policy) {}
    void get(const QUrl &url,int cacheSeconds,Validator validator={});
    void cancel();
    int statusCode() const { return m_statusCode; }
signals:
    void received(const QJsonDocument &document,const QDateTime &expiresAt);
    void failed(const QString &error);
private:
    RequestPolicy m_policy;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    quint64 m_generation=0;
    int m_statusCode=0;
};
}
