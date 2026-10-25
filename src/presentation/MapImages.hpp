#pragma once
#include "MeteoUiExport.hpp"
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
class QQmlEngine;
class QJSEngine;

class METEO_UI_EXPORT MapImages : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    static MapImages *create(QQmlEngine *engine, QJSEngine *);
    Q_INVOKABLE QUrl cached(const QUrl &url) const;
    Q_INVOKABLE QString cachedHtml(const QString &html, const QUrl &base = QUrl(QStringLiteral("https://www.meteo.pl/"))) const;
    Q_INVOKABLE void prefetch(const QUrl &url) const;
private:
    // A public default constructor would bypass create(), leaving the engine
    // without the image provider even though cached() returns provider URLs.
    explicit MapImages(QObject *parent = nullptr) : QObject(parent) {}
};
