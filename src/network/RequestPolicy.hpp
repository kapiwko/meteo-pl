#pragma once
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <algorithm>

namespace Meteo {
struct RequestPolicy {
    int inactivityMilliseconds = 15000;
    int totalMilliseconds = 20000;
    qint64 maximumBytes = 4 * 1024 * 1024;

    void configure(QNetworkRequest &request) const
    {
        request.setTransferTimeout(inactivityMilliseconds);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    }
    void enforce(QNetworkReply *reply) const
    {
        // This deadline is independent of progress, unlike transfer timeout.
        QTimer::singleShot(std::max(1,totalMilliseconds), reply, [reply] {
            if (reply->isFinished()) return;
            reply->setProperty("meteoDeadlineExceeded",true);
            reply->abort();
        });
        QObject::connect(reply,&QIODevice::readyRead,reply,[reply,limit=maximumBytes] {
            if (reply->bytesAvailable() > limit) reply->abort();
        });
        QObject::connect(reply,&QNetworkReply::metaDataChanged,reply,[reply,limit=maximumBytes] {
            if (reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > limit) reply->abort();
        });
    }
    static bool deadlineExceeded(const QNetworkReply *reply)
    { return reply->property("meteoDeadlineExceeded").toBool(); }
};
}
