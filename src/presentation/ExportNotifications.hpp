#pragma once
#include "MeteoUiExport.hpp"
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT ExportNotifications : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    explicit ExportNotifications(QObject *parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE void saved(const QUrl &file, const QString &title);
    Q_INVOKABLE bool openFile(const QUrl &file) const;
    Q_INVOKABLE bool openFolder(const QUrl &file) const;
};
