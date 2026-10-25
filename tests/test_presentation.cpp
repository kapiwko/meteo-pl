#include "WeatherModel.hpp"
#include "ChartConfiguration.hpp"
#include "GpuPlot.hpp"
#include "Fixture.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSGRendererInterface>
#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextImageFormat>
#include <QTextDocument>
#include <QTimeZone>
#include <QTemporaryDir>
#include <QTest>

class PresentationTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_cache;
private slots:
    void initTestCase()
    {
        QVERIFY(m_cache.isValid());
        QCoreApplication::setOrganizationName("MeteoTest");
        QCoreApplication::setApplicationName("MeteoTest");
        QQuickStyle::setStyle("org.kde.desktop");
        qputenv("XDG_CACHE_HOME", m_cache.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", (m_cache.path() + "/config").toUtf8());
        const auto hash = QCryptographicHash::hash("um4_60:52.229700:21.012200", QCryptographicHash::Sha256).toHex();
        QVERIFY(QDir().mkpath(m_cache.path() + "/meteo-pl"));
        QFile file(m_cache.path() + "/meteo-pl/" + hash + ".json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        auto fixture = QJsonDocument::fromJson(forecastFixture()).object();
        auto fields = fixture["data"].toObject();
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            auto field = it.value().toObject();
            const auto original = field["data"].toArray();
            QJsonArray values;
            for (int repeat = 0; repeat < 6; ++repeat) for (const auto &value : original) values.append(value);
            field["data"] = values;
            it.value() = field;
        }
        fixture["data"] = fields;
        const QJsonObject envelope{{"model", "um4_60"}, {"latitude", 52.2297}, {"longitude", 21.0122},
                                   {"savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                                   {"payload", QString::fromUtf8(QJsonDocument(fixture).toJson())}};
        file.write(QJsonDocument(envelope).toJson());
    }
    void loadsCachedForecastAndClearsItOnLocationChange()
    {
        WeatherModel model;
        model.setAutoRefresh(false);
        model.refresh(false);
        QVERIFY(model.hasData());
        QVERIFY(model.usingCache());
        QVERIFY(!model.busy());
        QVERIFY(!model.stale());
        const auto pressure = model.series()["slpres_point"].toList().first().toMap()["value"].toDouble();
        QCOMPARE(pressure, 1012.0);
        const auto clouds = model.series()["cldtot_aver"].toList().first().toMap()["value"].toDouble();
        QCOMPARE(clouds, 10.0);
        model.setLatitude(50.8661);
        QVERIFY(!model.hasData());
        QVERIFY(model.series().isEmpty());
        QCOMPARE(model.temperatureText(), QStringLiteral("—"));
    }
    void nativeGpuRendererDrawsClippedGeometry()
    {
        if (!qEnvironmentVariableIsSet("METEO_REQUIRE_GPU")) QSKIP("Separate GPU run checks native scene graph rendering");
        QQuickWindow window;
        window.setColor(Qt::white);
        window.resize(300, 200);
        GpuPlot plot(window.contentItem());
        plot.setSize({300, 200});
        connect(&plot, &GpuPlot::paint, &plot, [&] {
            plot.reset();
            plot.setProperty("fillStyle", QColor("#00aa00"));
            plot.fillRect(20,20,80,60);
            plot.save();
            plot.beginPath(); plot.rect(150,20,50,50); plot.clip();
            plot.setProperty("fillStyle", QColor("#1539c0"));
            plot.fillRect(100,0,200,100);
            plot.restore();
            plot.setProperty("strokeStyle", QColor("#f44336"));
            plot.setProperty("lineWidth", 4);
            plot.beginPath();plot.moveTo(20,150);plot.lineTo(250,150);plot.stroke();
        });
        window.show();
        plot.requestPaint();
        QTRY_VERIFY(window.rendererInterface()->graphicsApi() != QSGRendererInterface::Unknown);
        QVERIFY2(window.rendererInterface()->graphicsApi() != QSGRendererInterface::Software, "GPU test must use the RHI scene graph");
        QTRY_COMPARE(window.grabWindow().pixelColor(50,50), QColor("#00aa00"));
        const auto frame=window.grabWindow();
        QCOMPARE(frame.pixelColor(175,40),QColor("#1539c0"));
        QCOMPARE(frame.pixelColor(120,40),QColor(Qt::white));
        QCOMPARE(frame.pixelColor(100,150),QColor("#f44336"));
    }
    void rendersAllCharts()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop|overridden).*")));
        WeatherModel model;
        model.setAutoRefresh(false);
        QVERIFY(!model.hasData());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "ForecastPanel");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"weather", QVariant::fromValue(&model)}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(780, 1000);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(780, 1000));
        window.show();
        model.refresh(false);
        QVERIFY(model.hasData());
        QTest::qWait(300);
        const auto screenshot = window.grabWindow();
        QVERIFY(!screenshot.isNull());
        const auto screenshotPath = qEnvironmentVariable("METEO_SCREENSHOT");
        if (!screenshotPath.isEmpty())
            QVERIFY(screenshot.save(screenshotPath));
        const auto settingsScreenshot = qEnvironmentVariable("METEO_SETTINGS_SCREENSHOT");
        if (!settingsScreenshot.isEmpty()) {
            auto *settings = object->findChild<QObject *>("chartSettingsDialog");
            QVERIFY(settings);
            QVERIFY(QMetaObject::invokeMethod(settings, "open"));
            QTest::qWait(300);
            auto *editor = object->findChild<QObject *>("meteogramDisplaySettings");
            QVERIFY(editor);
            editor->setProperty("expandedChart", "temperature");
            editor->setProperty("expandedSeries", "temperature/airtmp_point");
            QTest::qWait(100);
            QVERIFY(window.grabWindow().save(settingsScreenshot));
            QVERIFY(QMetaObject::invokeMethod(settings, "close"));
        }
        const auto findItem = [](auto &&self, QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name) return parent;
            for (auto *child : parent->childItems()) if (auto *found = self(self, child, name)) return found;
            return nullptr;
        };
        auto *plot = findItem(findItem, item, "plot-temperature");
        QVERIFY(plot);
        auto *chart = plot->parentItem();
        const auto originalBounds = chart->property("bounds").value<QJSValue>().toVariant().toMap();
        QVERIFY(originalBounds.contains("high"));
        const QStringList seriesToToggle{"airtmp_point", "airtmp_max", "dwptmp_point", "wchill_point", "wchill_max", "grdtmp_min", "grdtmp_max"};
        for (const auto &key : seriesToToggle)
            QVERIFY(QMetaObject::invokeMethod(chart, "toggle", Q_ARG(QVariant, key)));
        QCOMPARE(chart->property("bounds").value<QJSValue>().toVariant().toMap(), originalBounds);
        for (const auto &key : seriesToToggle)
            QVERIFY(QMetaObject::invokeMethod(chart, "toggle", Q_ARG(QVariant, key)));
        QTest::qWait(50);
        auto *gpu = qobject_cast<GpuPlot *>(findItem(findItem, item, "gpu-base-temperature"));
        const auto baseFrames = gpu ? gpu->frameCount() : 0;
        const auto cursorPoint = plot->mapToScene(QPointF(plot->width() / 2, plot->height() / 2)).toPoint();
        QTest::mouseMove(&window, cursorPoint);
        QTRY_VERIFY(object->property("selectedTime").toDouble() > 0);
        auto *inspector = object->findChild<QObject *>("forecastInspector");
        QVERIFY(inspector);
        QTRY_VERIFY(inspector->property("visible").toBool());
        if (gpu) {
            QTest::qWait(50);
            QCOMPARE(gpu->frameCount(), baseFrames); // Moving the cursor must not rebuild the chart geometry.
        }
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, cursorPoint);
        QVERIFY(object->property("pinned").toBool());
        QTest::keyClick(&window, Qt::Key_Escape);
        QTRY_VERIFY(!inspector->property("visible").toBool());
        const auto fullSpan = object->property("viewportEnd").toDouble() - object->property("viewportStart").toDouble();
        QVERIFY(QMetaObject::invokeMethod(object.data(), "zoom", Q_ARG(QVariant, 0.5), Q_ARG(QVariant, model.firstTimestamp() + fullSpan / 2)));
        QVERIFY(object->property("viewportEnd").toDouble() - object->property("viewportStart").toDouble() < fullSpan);
    }
    void configuredChartsMixDifferentUnitsAndRespectProfileAxes()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        WeatherModel model;
        model.setAutoRefresh(false);
        model.refresh(false);
        QVERIFY(model.hasData());
        auto temperature = defaultSeriesStyle("airtmp_point");
        temperature["color"] = "#123456";
        temperature["lineWidth"] = 3.5;
        auto rain = defaultSeriesStyle("pcpttl_aver");
        rain["axis"] = "right";
        const QVariantList layout{QVariantMap{{"key", "chart:mixed"}, {"title", "Temperatura i deszcz"}, {"enabled", true},
            {"leftScale", "auto"}, {"rightScale", "auto"}, {"leftMin", -20}, {"leftMax", 40}, {"rightMax", 10},
            {"series", QVariantList{temperature, rain}}}};
        const auto charts = model.configuredCharts(layout);
        QCOMPARE(charts.size(), 1);
        const auto definition = charts.first().toMap();
        QCOMPARE(definition["leftScale"].toString(), "temperature");
        QCOMPARE(definition["rightScale"].toString(), "precipitation");
        const auto fields = definition["fields"].toList();
        QCOMPARE(fields.size(), 2);
        QCOMPARE(fields.first().toMap()["axis"].toString(), "left");
        QCOMPARE(fields.last().toMap()["axis"].toString(), "right");
        QCOMPARE(fields.first().toMap()["color"].toString(), "#123456");
        QCOMPARE(fields.first().toMap()["lineWidth"].toDouble(), 3.5);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "Meteogram");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> chart(component.createWithInitialProperties({{"weather", QVariant::fromValue(&model)},
            {"title", definition["title"]}, {"groupKey", definition["key"]}, {"fields", fields}, {"chartConfiguration", definition}}));
        QVERIFY2(chart, qPrintable(component.errorString()));
        auto bounds = chart->property("bounds").value<QJSValue>().toVariant().toMap();
        auto right = chart->property("rightBounds").value<QJSValue>().toVariant().toMap();
        QCOMPARE(bounds["low"].toDouble(), -20);
        QCOMPARE(bounds["high"].toDouble(), 40);
        QCOMPARE(right["low"].toDouble(), 0);
        QCOMPARE(right["high"].toDouble(), 10);
        QVariant leftUnit, rightUnit;
        QVERIFY(QMetaObject::invokeMethod(chart.data(), "axisUnit", Q_RETURN_ARG(QVariant,leftUnit), Q_ARG(QVariant,false)));
        QVERIFY(QMetaObject::invokeMethod(chart.data(), "axisUnit", Q_RETURN_ARG(QVariant,rightUnit), Q_ARG(QVariant,true)));
        QCOMPARE(leftUnit.toString(),QStringLiteral("°C"));
        QCOMPARE(rightUnit.toString(),QStringLiteral("mm"));
        QQuickWindow window;
        window.resize(780, 340);
        auto *item = qobject_cast<QQuickItem *>(chart.data());
        item->setParentItem(window.contentItem()); item->setSize({780, 340});
        window.show();
        QTest::qWait(100);
        QVERIFY(!window.grabWindow().isNull());
        QVERIFY(QMetaObject::invokeMethod(chart.data(), "toggle", Q_ARG(QVariant, "pcpttl_aver")));
        QCOMPARE(chart->property("bounds").value<QJSValue>().toVariant().toMap(), bounds);
        QCOMPARE(chart->property("rightBounds").value<QJSValue>().toVariant().toMap(), right);
    }
    void settingsReleaseSpaceAfterCollapsingChartsAndSeries()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "DisplaySettings");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(650, 1000);
        item->setParentItem(window.contentItem());
        item->setWidth(620);
        window.show();
        const auto findChart = [](auto &&self, QQuickItem *parent) -> QQuickItem * {
            if (parent->objectName() == "chart-settings-temperature") return parent;
            for (auto *child : parent->childItems()) if (auto *found = self(self, child)) return found;
            return nullptr;
        };
        auto *chart = findChart(findChart, item);
        QVERIFY(chart);
        QTRY_VERIFY(chart->height() > 0 && chart->height() < 100);
        QTest::qWait(100); // Let wrapped labels and the Flow of file buttons finish layout.
        const auto closedHeight = chart->height();
        const auto closedTotal = item->implicitHeight();
        for (int repeat = 0; repeat < 2; ++repeat) {
            object->setProperty("expandedChart", "temperature");
            QTRY_VERIFY(chart->height() > closedHeight + 250);
            const auto openHeight = chart->height();
            object->setProperty("expandedSeries", "temperature/airtmp_point");
            QTRY_VERIFY(chart->height() > openHeight + 250);
            object->setProperty("expandedSeries", "");
            QTRY_VERIFY(qAbs(chart->height() - openHeight) < 2);
            object->setProperty("expandedChart", "");
            QTRY_VERIFY(qAbs(chart->height() - closedHeight) < 2);
            QTRY_VERIFY(qAbs(item->implicitHeight() - closedTotal) < 2);
        }
    }
    void timeLabelsFitNarrowChartsWithoutOverlapping()
    {
        WeatherModel model;
        model.setAutoRefresh(false);
        model.refresh(false);
        const auto definition = model.chartGroups().first().toMap();
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "Meteogram");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> chart(component.createWithInitialProperties({{"weather", QVariant::fromValue(&model)},
            {"title", "Temperature"}, {"groupKey", "temperature"}, {"fields", definition["fields"]}}));
        QVERIFY2(chart, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(chart.data());
        QQuickWindow window;
        item->setParentItem(window.contentItem());
        window.resize(1100, 320);
        window.show();
        for (const int span : {60, 120, 240}) {
            chart->setProperty("endTime", model.firstTimestamp() + span * 3600);
            for (const int width : {1100, 780, 480, 260}) {
                item->setSize({qreal(width), 320});
                QTest::qWait(20);
                QVariant result;
                QVERIFY(QMetaObject::invokeMethod(chart.data(), "timeAxisLabels", Q_RETURN_ARG(QVariant, result)));
                const auto axis = result.value<QJSValue>().toVariant().toMap();
                const auto labels = axis["labels"].toList();
                QVERIFY(labels.size() >= 2);
                if (width <= 480) QCOMPARE(axis["format"].toString(), QStringLiteral("HH"));
                if (width == 1100 && span == 60) QCOMPARE(axis["format"].toString(), QStringLiteral("HH:mm"));
                for (const auto &key : {"labels", "days"}) {
                    double previousRight = 0;
                    for (const auto &entry : axis[key].toList()) {
                        const auto label = entry.toMap();
                        const auto left = label["x"].toDouble() - label["width"].toDouble() / 2;
                        const auto right = label["x"].toDouble() + label["width"].toDouble() / 2;
                        QVERIFY(left >= chart->property("leftMargin").toDouble() - 0.1);
                        QVERIFY(right <= width - chart->property("rightMargin").toDouble() + 0.1);
                        QVERIFY(left >= previousRight + 7.5);
                        if (QByteArray(key) == "days") {
                            const auto middle=(label["start"].toDouble()+label["end"].toDouble())/2;
                            QVariant position;
                            QVERIFY(QMetaObject::invokeMethod(chart.data(), "px", Q_RETURN_ARG(QVariant,position), Q_ARG(QVariant,middle)));
                            QVERIFY(qAbs(label["x"].toDouble()-position.toDouble())<0.01);
                        }
                        previousRight = right;
                    }
                }
            }
        }
    }
    void precipitationIntervalsTouchAndFogIsAboveCloudFill()
    {
        WeatherModel model;
        model.setAutoRefresh(false);
        model.setUtc(true);
        model.refresh(false);
        QVERIFY(model.hasData());
        const auto first = model.firstTimestamp();
        const auto points = [first](double value) {
            QVariantList result;
            for (int i = 0; i < 4; ++i) result.append(QVariantMap{{"time", first + i * 3600}, {"value", value}});
            return result;
        };
        const QVariantMap rain{{"key", "pcpttl_aver"}, {"label", "Rain"}, {"unit", "mm"}, {"color", "#00aa00"}, {"kind", "bars"}, {"points", points(1)}};
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "Meteogram");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> chart(component.createWithInitialProperties({{"weather", QVariant::fromValue(&model)},
            {"title", "Rain"}, {"groupKey", "precipitation"}, {"fields", QVariantList{rain}},
            {"startTime", first}, {"endTime", first + 4 * 3600}, {"nowTime", 0}}));
        QVERIFY2(chart, qPrintable(component.errorString()));
        QQuickWindow window;
        window.resize(780, 300);
        auto *item = qobject_cast<QQuickItem *>(chart.data());
        item->setParentItem(window.contentItem());
        item->setSize({780, 300});
        window.show();
        auto *plot = chart->findChild<QQuickItem *>("plot-precipitation");
        QVERIFY(plot);
        QVariant boundary;
        QVERIFY(QMetaObject::invokeMethod(chart.data(), "px", Q_RETURN_ARG(QVariant, boundary), Q_ARG(QVariant, first + 3600)));
        const auto pixel = plot->mapToScene({boundary.toDouble(), plot->height() - 40}).toPoint();
        QTRY_COMPARE(window.grabWindow().pixelColor(pixel), QColor("#00aa00"));
        const auto image = window.grabWindow();
        for (int offset = -2; offset <= 2; ++offset) QCOMPARE(image.pixelColor(pixel + QPoint(offset, 0)), QColor("#00aa00"));
        const QVariantMap fog{{"key", "fog_max"}, {"label", "Fog"}, {"unit", "%"}, {"color", "#ffb400"}, {"kind", "rightBars"}, {"points", points(100)}};
        const QVariantMap clouds{{"key", "cldlow_aver"}, {"label", "Clouds"}, {"unit", "%"}, {"color", "#5a5a5a"}, {"kind", "cloudArea"}, {"points", points(100)}};
        chart->setProperty("groupKey", "clouds");
        chart->setProperty("fields", QVariantList{fog, clouds}); // deliberately reversed layer order
        QVERIFY(QMetaObject::invokeMethod(chart.data(), "px", Q_RETURN_ARG(QVariant, boundary), Q_ARG(QVariant, first + 3600)));
        const auto fogPixel = plot->mapToScene({boundary.toDouble(), plot->height() - 40}).toPoint();
        QTRY_COMPARE(window.grabWindow().pixelColor(fogPixel), QColor("#ffb400"));

        // Opaque fills used to erase the grid on these four charts. A grid
        // column must remain distinguishable from the same fill beside it.
        struct FilledChart { const char *group, *key, *kind, *color; double value; };
        const FilledChart cases[]{{"precipitation", "pcpttl_aver", "bars", "#00aa00", 2.2},
                                  {"pressure", "slpres_point", "line", "#000000", 1010.2},
                                  {"wind", "wind10_sd_true_prev_point", "line", "#111199", 2.2},
                                  {"clouds", "cldlow_aver", "cloudArea", "#5a5a5a", 100}};
        for (const auto &test : cases) {
            chart->setProperty("groupKey", test.group);
            chart->setProperty("fields", QVariantList{QVariantMap{{"key", test.key}, {"label", "Fill"},
                {"unit", ""}, {"color", test.color}, {"kind", test.kind}, {"points", points(test.value)}}});
            const auto gridVisible = [&] {
                QVariant gridX;
                QMetaObject::invokeMethod(chart.data(), "px", Q_RETURN_ARG(QVariant, gridX), Q_ARG(QVariant, first + 3 * 3600));
                const auto origin = plot->mapToScene({gridX.toDouble(), plot->height() - 50}).toPoint();
                const auto frame = window.grabWindow();
                int different = 0;
                for (int y = 0; y < 16; ++y) {
                    const auto reference = frame.pixelColor(origin + QPoint(8, y));
                    if (frame.pixelColor(origin + QPoint(-1, y)) != reference || frame.pixelColor(origin + QPoint(0, y)) != reference)
                        ++different;
                }
                return different >= 3;
            };
            QTRY_VERIFY2(gridVisible(), test.group);
        }
    }
    void freezingBackgroundFollowsZeroOnBothTemperatureCharts()
    {
        WeatherModel model;
        model.setAutoRefresh(false);
        model.refresh(false);
        const auto first = model.firstTimestamp();
        const QVariantList points{QVariantMap{{"time", first}, {"value", -5}},
                                  QVariantMap{{"time", first + 3600}, {"value", 5}}};
        const QVariantMap temperature{{"key", "airtmp_point"}, {"label", "Temperature"}, {"unit", "°C"},
            {"color", "#f44336"}, {"kind", "line"}, {"points", points}};
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "Meteogram");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> chart(component.createWithInitialProperties({{"weather", QVariant::fromValue(&model)},
            {"title", "Temperature"}, {"groupKey", "temperature"}, {"fields", QVariantList{temperature}},
            {"startTime", first}, {"endTime", first + 3600}, {"nowTime", 0}}));
        QVERIFY2(chart, qPrintable(component.errorString()));
        QQuickWindow window;
        window.setColor(Qt::white);
        window.resize(780, 300);
        auto *item = qobject_cast<QQuickItem *>(chart.data());
        item->setParentItem(window.contentItem());
        item->setSize({780, 300});
        window.show();
        auto *plot = chart->findChild<QQuickItem *>("plot-temperature");
        QVERIFY(plot);
        const auto pixelFor = [&](double value) {
            QVariant x, y;
            QMetaObject::invokeMethod(chart.data(), "px", Q_RETURN_ARG(QVariant, x), Q_ARG(QVariant, first + 600));
            QMetaObject::invokeMethod(chart.data(), "py", Q_RETURN_ARG(QVariant, y), Q_ARG(QVariant, value), Q_ARG(QVariant, false));
            return plot->mapToScene({x.toDouble(), y.toDouble()}).toPoint();
        };
        for (const auto &group : {"temperature", "ground"}) {
            chart->setProperty("groupKey", group);
            QTRY_COMPARE(window.grabWindow().pixelColor(pixelFor(-2.7)), QColor("#87ceeb"));
            QCOMPARE(window.grabWindow().pixelColor(pixelFor(2.1)), QColor(Qt::white));
        }
        auto positive = temperature;
        positive["points"] = QVariantList{QVariantMap{{"time", first}, {"value", 2}},
                                        QVariantMap{{"time", first + 3600}, {"value", 5}}};
        chart->setProperty("fields", QVariantList{positive});
        QTRY_COMPARE(window.grabWindow().pixelColor(pixelFor(2.1)), QColor(Qt::white));
        auto negative = temperature;
        negative["points"] = QVariantList{QVariantMap{{"time", first}, {"value", -8}},
                                        QVariantMap{{"time", first + 3600}, {"value", -2}}};
        chart->setProperty("fields", QVariantList{negative});
        QTRY_COMPARE(window.grabWindow().pixelColor(pixelFor(-4.7)), QColor("#87ceeb"));
    }
    void articleImagesFitAndKeepTheirAspectRatioAfterResizing()
    {
        QTemporaryDir files;
        QVERIFY(files.isValid());
        QImage image(1800,600,QImage::Format_RGB32);
        image.fill(QColor("#239876"));
        const auto path=files.filePath("large.png");
        QVERIFY(image.save(path));
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import QtQuick.Controls
            import pl.meteo.desktop
            TextArea {
                id: body
                readOnly: true
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.Wrap
                ResponsiveImages { document: body.textDocument; maximumWidth: body.width - body.leftPadding - body.rightPadding }
            })", QUrl());
        QVERIFY2(component.isReady(),qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"text", "<p>Before</p><img src='"+QUrl::fromLocalFile(path).toString()+"' width='1800' height='600'><p>After</p>"}}));
        QVERIFY2(object,qPrintable(component.errorString()));
        auto *item=qobject_cast<QQuickItem *>(object.data());
        auto *document=qvariant_cast<QQuickTextDocument *>(object->property("textDocument"))->textDocument();
        const auto imageFormat=[document] {
            for (auto block=document->begin();block.isValid();block=block.next()) for (auto it=block.begin();!it.atEnd();++it)
                if (it.fragment().charFormat().isImageFormat()) return it.fragment().charFormat().toImageFormat();
            return QTextImageFormat();
        };
        for (const int width : {600,300,800}) {
            item->setWidth(width);
            const auto available=width-object->property("leftPadding").toDouble()-object->property("rightPadding").toDouble();
            QTRY_VERIFY(qAbs(imageFormat().width()-available)<1);
            QVERIFY(qAbs(imageFormat().width()/imageFormat().height()-3)<0.001);
        }
        object->setProperty("text", "<p>Without explicit dimensions</p><img src='"+QUrl::fromLocalFile(path).toString()+"'>");
        const auto available=item->width()-object->property("leftPadding").toDouble()-object->property("rightPadding").toDouble();
        QTRY_VERIFY(qAbs(imageFormat().width()-available)<1);
        QVERIFY(qAbs(imageFormat().width()/imageFormat().height()-3)<0.001);
    }
    void calendarBoundariesAreMidnightsInTheSelectedTimeZone()
    {
        WeatherModel model;
        model.setAutoRefresh(false);
        const auto zone=QTimeZone::systemTimeZone();
        const auto first=QDate(2026,10,24).startOfDay(zone).toSecsSinceEpoch();
        const auto end=QDate(2026,10,27).startOfDay(zone).toSecsSinceEpoch();
        const auto days=model.calendarDays(first,end);
        QCOMPARE(days.size(),3);
        for (const auto &entry:days) {
            const auto day=entry.toMap();
            QCOMPARE(model.formatTime(day["start"].toDouble(),"HH:mm"),QStringLiteral("00:00"));
            QCOMPARE(model.formatTime(day["end"].toDouble(),"HH:mm"),QStringLiteral("00:00"));
        }
        if (zone.id()=="Europe/Warsaw") QCOMPARE(days[1].toMap()["end"].toLongLong()-days[1].toMap()["start"].toLongLong(),25*3600);
        model.setUtc(true);
        const auto utc=model.calendarDays(first,end);
        for (const auto &entry:utc) {
            const auto day=entry.toMap();
            QCOMPARE(model.formatTime(day["start"].toDouble(),"HH:mm"),QStringLiteral("00:00"));
            QCOMPARE(day["end"].toLongLong()-day["start"].toLongLong(),24*3600);
        }
    }
    void detailsAndUnitsUseMatchingTimestamps()
    {
        WeatherModel model;
        model.setAutoRefresh(false);
        model.refresh(false);
        const auto time = model.firstTimestamp() + 3600;
        const auto valueFor = [](const QVariantList &rows, const QString &key) {
            for (const auto &row : rows) if (row.toMap()["key"] == key) return row.toMap()["number"];
            return QVariant();
        };
        QCOMPARE(valueFor(model.detailsAt(time), "airtmp_point").toDouble(), 12.0);
        QCOMPARE(valueFor(model.detailsAt(time), "slpres_point").toDouble(), 1013.0);
        QCOMPARE(valueFor(model.detailsAt(time), "visibl_min").toDouble(), 10.0);
        model.setWindUnit("km/h");
        QCOMPARE(valueFor(model.detailsAt(time), "wind10_sd_true_prev_point").toDouble(), 10.8);
        QCOMPARE(model.nearestTimestamp(time + 1000), time);
        QCOMPARE(model.nearestTimestamp(time + 2000), time + 3600);
        QVERIFY(!model.dailyForecast().isEmpty());
        QVERIFY(model.chartGroups().size() >= 9);
    }
    void applicationConfigurationHasPaddingAndFitsWindow()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/apps/meteo/Main.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"smokeTest", true}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.data());
        QVERIFY(window);
        auto *dialog = object->findChild<QObject *>("locationDialog");
        QVERIFY(dialog);
        if (auto *model = object->findChild<WeatherModel *>()) model->refresh(false);
        QTest::qWait(150);
        const auto appScreenshot = qEnvironmentVariable("METEO_APP_SCREENSHOT");
        if (!appScreenshot.isEmpty()) QVERIFY(window->grabWindow().save(appScreenshot));
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTest::qWait(150);
        QVERIFY(dialog->property("visible").toBool());
        QVERIFY(dialog->property("padding").toDouble() >= 16);
        QVERIFY(dialog->property("height").toDouble() < window->height());
        const auto screenshotPath = qEnvironmentVariable("METEO_DIALOG_SCREENSHOT");
        if (!screenshotPath.isEmpty()) QVERIFY(window->grabWindow().save(screenshotPath));
        QMetaObject::invokeMethod(dialog, "close");
    }
    void plasmaConfigurationUpdatesInBothDirections()
    {
        QQmlEngine engine;
        const auto path = QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/configGeneral.qml");
        QQmlComponent component(&engine, QUrl::fromLocalFile(path));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));
        editor->setProperty("cfg_locationName", "Kielce");
        editor->setProperty("cfg_latitude", 50.8661);
        editor->setProperty("cfg_longitude", 20.6286);
        QCOMPARE(editor->property("locationName").toString(), "Kielce");
        QCOMPARE(editor->property("latitude").toDouble(), 50.8661);
        QVERIFY(editor->property("isSaveAllowed").toBool());
        editor->setProperty("latitude", 40.7);
        QCOMPARE(editor->property("cfg_latitude").toDouble(), 40.7);
        QVERIFY(!editor->property("isSaveAllowed").toBool());
        editor->setProperty("model", "gfs");
        QVERIFY(editor->property("isSaveAllowed").toBool());
        QCOMPARE(editor->property("cfg_model").toString(), "gfs");
    }
};
QTEST_MAIN(PresentationTest)
#include "test_presentation.moc"
