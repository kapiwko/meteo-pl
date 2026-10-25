#pragma once
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class ReadingsExporter : public QObject {
    Q_OBJECT
    QML_ELEMENT
public:
    using QObject::QObject;
    Q_INVOKABLE QVariantMap saveCsv(const QUrl &file, const QVariantList &readings, const QVariantMap &metadata) const;
};
