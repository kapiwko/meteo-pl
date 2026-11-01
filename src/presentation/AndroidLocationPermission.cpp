#include <QtGlobal>
#ifdef Q_OS_ANDROID
#include "AndroidLocationPermission.hpp"
#include <QtCore/qcoreapplication_platform.h>
#include <QJniObject>
#include <QTimer>
#include <memory>

void requestAndroidLocationPermission(QObject *context, std::function<void(Qt::PermissionStatus)> completed)
{
    auto ticket = std::make_shared<QJniObject>();
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([ticket] {
        const QJniObject activity = QNativeInterface::QAndroidApplication::context();
        *ticket = activity.callObjectMethod(
            "requestMeteoLocationPermission", "()Ljava/util/concurrent/atomic/AtomicInteger;");
    }).then(context, [context, ticket, completed] {
        if (!ticket->isValid()) { completed(Qt::PermissionStatus::Denied); return; }
        // Reading an AtomicInteger is thread-safe and never calls the UI
        // thread. Both event loops remain free while the user decides.
        auto *poll = new QTimer(context);
        QObject::connect(poll, &QTimer::timeout, context, [ticket, poll, completed] {
            const auto result = ticket->callMethod<jint>("get");
            if (result < 0) return;
            poll->stop();
            poll->deleteLater();
            completed(result == 1 ? Qt::PermissionStatus::Granted : Qt::PermissionStatus::Denied);
        });
        poll->start(50);
    });
}
#endif
