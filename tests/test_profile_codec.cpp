#include "presentation/ChartConfiguration.hpp"
#include "presentation/ProfileFileCodec.hpp"
#include "presentation/JsonSchemaValidator.hpp"
#include <QFile>
#include <QColor>
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
    void rangePartnersMatchTheirQuantityUnitAndRole()
    {
        for (const auto &key : {"airtmp_max","wchill_max","grdtmp_max"}) {
            const auto standard=defaultSeriesStyle(key);
            QVERIFY(isRangePartner(key,standard["lowerKey"].toString()));
        }
        for (const auto &lower : {"slpres_point","wind10_dr_deg_true_prev_point","wchill_min","airtmp_point","airtmp_max","missing"}) {
            auto style=defaultSeriesStyle("airtmp_max"); style["lowerKey"]=lower; style["render"]="range";
            QVERIFY(!isRangePartner("airtmp_max",lower));
            QCOMPARE(normalizeSeriesStyle(style)["lowerKey"].toString(),"airtmp_min");
            auto chart=defaultChartLayout("classic").first().toMap(); chart["series"]=QVariantList{style};
            QString error;
            QVERIFY(!ProfileFileCodec::encode({QVariantMap{{"name","Invalid range"},{"charts",QVariantList{chart}}}},&error));
            QVERIFY(!error.isEmpty());
        }
    }
    void fillsAreCompatibleWithTheirQuantityAndPalette()
    {
        for (const auto &entry : chartSeriesCatalog()) {
            const auto standard=entry.toMap();
            for (const auto &render : seriesRenderers(standard["key"].toString())) for (const auto &fill : {"solid","pressure","wind"}) {
                auto style=standard; style["render"]=render; style["fill"]=fill;
                const auto resolved=normalizeSeriesStyle(style);
                QVERIFY(seriesFills(style["key"].toString()).contains(resolved["fill"].toString()));
                if (resolved["fill"]!="solid") {
                    QVERIFY(!resolved["palette"].toList().isEmpty());
                    QVERIFY(!resolved["paletteFallback"].toMap().isEmpty());
                }
            }
        }
        QCOMPARE(seriesFills("airtmp_point"),QStringList{"solid"});
        QVERIFY(!seriesFills("wind10_sd_true_prev_point").contains("pressure"));
    }
    void microscopicDashPatternsAreRejectedAtImport()
    {
        auto chart=defaultChartLayout("classic").first().toMap();
        auto appearance=chart["appearance"].toMap(); auto light=appearance["light"].toMap();
        light["gridDash"]=QVariantList{1e-9}; appearance["light"]=light; chart["appearance"]=appearance;
        QString error;
        QVERIFY(!ProfileFileCodec::encode({QVariantMap{{"name","Tiny"},{"charts",QVariantList{chart}}}},&error));
        QVERIFY(!error.isEmpty());
        const auto recovered=normalizeChartLayout({chart}).first().toMap()["appearance"].toMap()["light"].toMap()["gridDash"].toList();
        QCOMPARE(recovered,QVariantList({0.5,0.5}));
    }
    void alphaSurvivesImportAndExport()
    {
        for (const auto &color : {QStringLiteral("#00112233"), QStringLiteral("#80112233"), QStringLiteral("#ff112233")}) {
            auto chart = defaultChartLayout("classic").first().toMap();
            auto styles = chart["series"].toList(); auto style = styles.first().toMap();
            style["color"] = color; style["darkColor"] = color; style["opacity"] = 0.4;
            styles[0] = style; chart["series"] = styles;
            QString error;
            const auto bytes = ProfileFileCodec::encode({QVariantMap{{"name", "Alpha"}, {"charts", QVariantList{chart}}}}, &error);
            QVERIFY2(bytes, qPrintable(error));
            const auto decoded = ProfileFileCodec::decode(*bytes, &error);
            QVERIFY2(decoded, qPrintable(error));
            const auto result = decoded->first().toMap()["charts"].toList().first().toMap()["series"].toList().first().toMap();
            QCOMPARE(QColor(result["color"].toString()), QColor(color));
            QCOMPARE(QColor(result["darkColor"].toString()), QColor(color));
            QCOMPARE(result["opacity"].toDouble(), 0.4);
        }
    }
    void bundledPresetsUseTheImportFormatWithoutDisabledTemplates()
    {
        const auto presets = builtinChartProfiles();
        QCOMPARE(presets.size(), 2);
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
        const auto quick = defaultChartLayout("basic");
        QCOMPARE(quick.size(), 3);
        QStringList quickKeys;
        for (const auto &chart : quick) quickKeys.append(chart.toMap()["key"].toString());
        QCOMPARE(quickKeys, (QStringList{"temperature", "precipitation", "wind"}));
        QVERIFY(quick.first().toMap()["series"].toList().size() < defaultChartLayout("classic").first().toMap()["series"].toList().size());
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
