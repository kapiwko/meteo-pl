#pragma once
#include "MeteoUiExport.hpp"
#include "core/WebLink.hpp"
#include <QDesktopServices>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class METEO_UI_EXPORT WebLinks : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    using QObject::QObject;
    Q_INVOKABLE bool open(const QUrl &link,const QUrl &base=QUrl(QStringLiteral("https://www.meteo.pl/"))) const
    {
        const auto resolved=Meteo::resolveWebLink(link,base);
        return !resolved.isEmpty() && QDesktopServices::openUrl(resolved);
    }
};
