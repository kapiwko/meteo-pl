#include "ForecastFields.hpp"
#include "ChartConfiguration.hpp"
#include "JsonSchemaValidator.hpp"
#include <QJsonArray>
#include <QJsonObject>

const QVector<ForecastField> &forecastFields()
{
    static const QVector<ForecastField> fields = [] {
        QVector<ForecastField> result;
        const auto values = bundledChartJson(":/meteo/charts/fields.json").object()["fields"].toArray();
        for (const auto &value : values) {
            const auto entry = value.toObject();
            ForecastField field;
            field.key = entry["key"].toString();
            field.label = entry["label"].toString();
            field.unit = entry["unit"].toString();
            field.group = entry["group"].toString();
            field.color = entry["color"].toString();
            field.kind = entry["kind"].toString();
            field.scale = entry["scale"].toDouble();
            field.decimals = entry["decimals"].toInt();
            field.darkColor = entry["darkColor"].toString();
            field.axisScale = entry["axisScale"].toString();
            field.renderers = entry["renderers"].toVariant().toStringList();
            result.append(field);
        }
        return result;
    }();
    return fields;
}

QString forecastDarkColor(const QString &lightColor, const QString &fieldKey)
{
    for (const auto &field : forecastFields()) if (field.key == fieldKey) return field.darkColor;
    return lightColor;
}

const QVector<ForecastChart> &forecastCharts()
{
    static const QVector<ForecastChart> charts = [] {
        QVector<ForecastChart> result;
        const auto values = bundledChartJson(":/meteo/charts/catalog.json").object()["charts"].toArray();
        for (const auto &value : values) {
            const auto entry = value.toObject();
            result.append({entry["key"].toString(), entry["title"].toString(), chartFieldKeys(entry["series"].toArray().toVariantList())});
        }
        return result;
    }();
    return charts;
}
