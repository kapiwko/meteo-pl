#pragma once
#include <QtGlobal>
#ifdef Q_OS_ANDROID
#include <QObject>
#include <functional>

void requestAndroidLocationPermission(QObject *context, std::function<void(Qt::PermissionStatus)> completed);
#endif
