#include "JsonSchemaValidator.hpp"
#include "core/Localization.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>
#include <valijson/adapters/qtjson_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

namespace {
const valijson::Schema &chartSchema()
{
    static const auto schema = [] {
        QFile file(":/meteo/schemas/chart-data.schema.json");
        if (!file.open(QIODevice::ReadOnly)) qFatal("Missing bundled chart schema");
        const QJsonValue document(QJsonDocument::fromJson(file.readAll()).object());
        auto schema = std::make_unique<valijson::Schema>();
        valijson::SchemaParser parser;
        parser.populateSchema(valijson::adapters::QtJsonAdapter(document), *schema);
        return schema;
    }();
    return *schema;
}
QJsonValue translateLabels(const QJsonValue &value)
{
    if (value.isArray()) {
        QJsonArray result;
        for (const auto &entry : value.toArray()) result.append(translateLabels(entry));
        return result;
    }
    if (!value.isObject()) return value;
    auto result = value.toObject();
    for (auto it = result.begin(); it != result.end(); ++it) {
        if ((it.key() == "name" || it.key() == "label" || it.key() == "title") && it.value().isString()) {
            const auto source = it.value().toString().toUtf8();
            it.value() = Meteo::i18n(source.constData());
        } else it.value() = translateLabels(it.value());
    }
    return result;
}
}

bool validateChartJson(const QJsonValue &value, QString *error)
{
    if (error) error->clear();
    valijson::Validator validator;
    valijson::ValidationResults results;
    if (validator.validate(chartSchema(), valijson::adapters::QtJsonAdapter(value), &results)) return true;
    if (error) {
        valijson::ValidationResults::Error failure;
        // The first error contains the affected object/property; avoid dumping
        // every alternative of the schema's document types into the UI.
        if (results.popError(failure)) {
            QStringList context;
            for (const auto &entry : failure.context) context.append(QString::fromStdString(entry));
            *error = Meteo::i18n("Invalid chart data at %1: %2").arg(context.join(QString()), QString::fromStdString(failure.description));
        } else *error = Meteo::i18n("The chart data does not match the JSON schema.");
    }
    return false;
}

QJsonDocument bundledChartJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) qFatal("Missing bundled chart data: %s", qPrintable(path));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonValue value = document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object());
    QString error;
    if (parseError.error != QJsonParseError::NoError || !validateChartJson(value, &error))
        qFatal("Invalid bundled chart data %s: %s", qPrintable(path), qPrintable(error));
    const auto translated = translateLabels(value);
    return document.isArray() ? QJsonDocument(translated.toArray()) : QJsonDocument(translated.toObject());
}
