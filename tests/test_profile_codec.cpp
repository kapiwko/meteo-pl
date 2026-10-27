#include "presentation/ChartConfiguration.hpp"
#include "presentation/ProfileFileCodec.hpp"
#include "presentation/JsonSchemaValidator.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

class ProfileCodecTest : public QObject
{
    Q_OBJECT
private slots:
    void bundledPresetsUseTheImportFormatWithoutDisabledTemplates()
    {
        const auto presets = builtinChartProfiles();
        QVERIFY(presets.size() >= 5);
        QSet<QString> ids;
        for (const auto &value : presets) {
            const auto preset = value.toMap();
            const auto id = preset["id"].toString();
            QVERIFY(!id.isEmpty() && !ids.contains(id));
            ids.insert(id);
            QVERIFY(preset["revision"].toInt() > 0);
            QFile file(":/meteo/profiles/" + id + ".json");
            QVERIFY(file.open(QIODevice::ReadOnly));
            QString error;
            const auto bytes = file.readAll();
            const auto decoded = ProfileFileCodec::decode(bytes, &error);
            QVERIFY2(decoded, qPrintable(error));
            const auto charts = decoded->first().toMap()["charts"].toList();
            QCOMPARE(charts, defaultChartLayout(id));
            QVERIFY(!charts.isEmpty());
            QSet<QString> keys;
            for (const auto &entry : charts) {
                const auto chart = entry.toMap();
                QVERIFY(chart["enabled"].toBool());
                QVERIFY(!keys.contains(chart["key"].toString()));
                keys.insert(chart["key"].toString());
            }
            QVERIFY(!keys.contains("windDirection")); // The arrows belong to Wind.
            const auto rawCharts = QJsonDocument::fromJson(bytes).object()["profiles"].toArray().first().toObject()["charts"].toArray();
            for (const auto &value : rawCharts) {
                const auto rawChart = value.toObject();
                QVERIFY(rawChart.contains("appearance"));
                for (const auto &raw : rawChart["series"].toArray()) {
                    const auto style = raw.toObject();
                    for (const auto &property : defaultSeriesStyle(style["key"].toString()).keys())
                        QVERIFY2(style.contains(property), qPrintable(id + ": " + style["key"].toString() + " is missing " + property));
                }
            }
        }
        QVERIFY(defaultChartLayout("basic").size() < defaultChartLayout("classic").size());
        QVERIFY(defaultChartLayout("aviation") != defaultChartLayout("classic"));
    }
    void templatesOutsideTheClassicPresetRemainImportable()
    {
        const auto catalog = defaultChartCatalog();
        QCOMPARE(normalizeChartLayout(catalog).size(), catalog.size());
        bool ground = false, direction = false;
        for (const auto &entry : catalog) {
            ground |= entry.toMap()["key"] == "ground";
            direction |= entry.toMap()["key"] == "windDirection";
        }
        QVERIFY(ground && direction);
        QString error;
        const auto encoded = ProfileFileCodec::encode({QVariantMap{{"name", "Templates"}, {"charts", catalog}}}, &error);
        QVERIFY2(encoded, qPrintable(error));
        const auto decoded = ProfileFileCodec::decode(*encoded, &error);
        QVERIFY2(decoded, qPrintable(error));
        QCOMPARE(decoded->first().toMap()["charts"].toList().size(), catalog.size());
    }
    void schemaRejectsMalformedAppearanceBeforeImport()
    {
        const auto definition = defaultChartLayout("classic").first().toMap();
        for (const auto &patch : {QVariantMap{{"opacity", 2.0}}, QVariantMap{{"render", "unknown"}},
                                 QVariantMap{{"color", "not-a-color"}}, QVariantMap{{"lineWidth", "thick"}}}) {
            auto chart = definition;
            auto styles = chart["series"].toList();
            auto style = styles.first().toMap();
            for (auto it = patch.cbegin(); it != patch.cend(); ++it) style[it.key()] = it.value();
            styles[0] = style;
            chart["series"] = styles;
            const auto bytes = QJsonDocument::fromVariant(QVariantMap{{"format", "pl.meteo.chart-profiles"}, {"version", 1},
                {"profiles", QVariantList{QVariantMap{{"name", "Invalid"}, {"charts", QVariantList{chart}}}}}}).toJson();
            QString error;
            QVERIFY(!ProfileFileCodec::decode(bytes, &error));
            QVERIFY(!error.isEmpty());
        }
    }
    void roundTripWithoutAnApplicationWindow()
    {
        const QVariantList profiles{
            QVariantMap{{"name", "My charts"}, {"charts", defaultChartLayout("classic")}, {"chartHeight", 220}}};
        QString error;
        const auto bytes = ProfileFileCodec::encode(profiles, &error);
        QVERIFY2(bytes.has_value(), qPrintable(error));
        const auto decoded = ProfileFileCodec::decode(*bytes, &error);
        QVERIFY2(decoded.has_value(), qPrintable(error));
        QCOMPARE(*decoded, profiles);
        QTemporaryDir directory;
        const auto file = QUrl::fromLocalFile(directory.filePath("profiles.json"));
        QVERIFY(ProfileFileCodec::write(file, profiles, &error));
        QCOMPARE(ProfileFileCodec::read(file), decoded);
    }
    void importedSemanticRenderersAreRestrictedToTheirQuantities()
    {
        auto layout = defaultChartLayout("classic");
        auto chart = layout.first().toMap();
        QVariantList invalid;
        for (const auto &key : {"airtmp_point", "pcpttl_type_max", "wind10_dr_deg_true_prev_point", "storm_max", "flash_max"}) {
            auto style = defaultSeriesStyle(key);
            style["render"] = QString(key) == "flash_max" ? "direction" : "flash";
            invalid.append(style);
        }
        chart["series"] = invalid;
        layout[0] = chart;
        const auto json = QJsonDocument(QJsonObject{{"format", "pl.meteo.chart-profiles"}, {"version", 1},
            {"profiles", QJsonArray{QJsonObject{{"name", "Imported"}, {"charts", QJsonArray::fromVariantList(layout)}}}}}).toJson();
        QString error;
        const auto decoded = ProfileFileCodec::decode(json, &error);
        QVERIFY2(decoded, qPrintable(error));
        const auto styles = decoded->first().toMap()["charts"].toList().first().toMap()["series"].toList();
        const QStringList expected{"line", "type", "direction", "storm", "flash"};
        for (int i = 0; i < expected.size(); ++i) QCOMPARE(styles[i].toMap()["render"].toString(), expected[i]);
        QVERIFY(!seriesRenderers("airtmp_point").contains("flash"));
        QVERIFY(seriesRenderers("airtmp_max").contains("rangeStems"));
        QVERIFY(!seriesRenderers("pcpttl_aver").contains("direction"));
        QCOMPARE(normalizeSeriesStyle({{"key", "airtmp_point"}, {"render", "bars"}})["render"].toString(), "bars");
    }
    void rejectsTheWholeFileIfTheLastProfileIsInvalid()
    {
        const auto charts = QJsonArray::fromVariantList(defaultChartLayout("classic"));
        QJsonObject invalidChart{{"key", "unknown"},
                                 {"series", QJsonArray{QJsonObject{{"key", "not-a-weather-field"}}}}};
        const auto bytes =
            QJsonDocument(QJsonObject{{"format", "pl.meteo.chart-profiles"},
                                      {"version", 1},
                                      {"profiles", QJsonArray{QJsonObject{{"name", "Valid"}, {"charts", charts}},
                                                              QJsonObject{{"name", "Invalid"},
                                                                          {"charts", QJsonArray{invalidChart}}}}}})
                .toJson();
        QString error;
        QVERIFY(!ProfileFileCodec::decode(bytes, &error));
        QVERIFY(!error.isEmpty());
        QTemporaryDir directory;
        const auto path = directory.filePath("profiles.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("existing"), 8);
        file.close();
        QVERIFY(!ProfileFileCodec::write(QUrl::fromLocalFile(path), {QVariantMap{{"name", "Invalid"}}}, &error));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("existing"));
    }
    void boundsInputAndRequiresALocalRegularFile()
    {
        QString error;
        QVERIFY(!ProfileFileCodec::decode(QByteArray(ProfileFileCodec::maximumBytes + 1, ' '), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!ProfileFileCodec::read(QUrl("https://example.org/profiles.json"), &error));
        QTemporaryDir directory;
        QVERIFY(!ProfileFileCodec::read(QUrl::fromLocalFile(directory.path()), &error));
        const auto profile = QVariantMap{{"name", "Profile"}, {"charts", defaultChartLayout("classic")}};
        QVariantList tooMany;
        for (int i = 0; i < 101; ++i)
            tooMany.append(profile);
        QVERIFY(!ProfileFileCodec::encode(tooMany, &error));
    }
};
QTEST_GUILESS_MAIN(ProfileCodecTest)
#include "test_profile_codec.moc"
