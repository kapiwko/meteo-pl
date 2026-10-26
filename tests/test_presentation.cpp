#include "WeatherModel.hpp"
#include "ChartConfiguration.hpp"
#include "ChartExporter.hpp"
#include "ReadingsExporter.hpp"
#include "GpuPlot.hpp"
#include "UserPreferences.hpp"
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
#include <QSignalSpy>
#include <QScopeGuard>

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
            for (int repeat = 0; repeat < 12; ++repeat) for (const auto &value : original) values.append(value);
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
    void changingLocationShowsLoadingBeforeTheRequestStarts()
    {
        WeatherModel model;
        model.setAutoRefresh(false);
        model.refresh(false);
        QVERIFY(model.hasData());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "ForecastHeader");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> header(component.createWithInitialProperties({{"weather", QVariant::fromValue(&model)}}));
        QVERIFY(header);
        auto *symbol = header->findChild<QObject *>("headerWeatherSymbol");
        QVERIFY(symbol);
        auto *indicator = symbol->findChild<QObject *>("forecastLoadingIndicator");
        auto *icon = symbol->findChild<QObject *>("forecastWeatherIcon");
        auto *time = header->findChild<QObject *>("forecastSummaryTime");
        QVERIFY(indicator); QVERIFY(icon); QVERIFY(time);
        QVERIFY(!header->property("loading").toBool());
        QVERIFY(!indicator->property("running").toBool());
        QVERIFY(!icon->property("source").toString().isEmpty());
        model.setAutoRefresh(true);
        QVERIFY(!header->property("loading").toBool()); // Background refresh keeps the weather icon.
        model.setLocationName("Kielce"); model.setLatitude(50.8661); model.setLongitude(20.6286);
        // Do not process events: the deferred request has not started, and there must be no placeholder flash.
        QVERIFY(!model.busy());
        QVERIFY(!model.hasData());
        QVERIFY(header->property("loading").toBool());
        QVERIFY(indicator->property("running").toBool());
        QVERIFY(icon->property("source").toString().isEmpty());
        QCOMPARE(time->property("text").toString(), "Downloading forecast…");
        model.setAutoRefresh(false); // Cancel the deferred request; this regression test stays offline.
        QVERIFY(!header->property("loading").toBool());
        model.setLocationName("");
        QVERIFY(!indicator->property("running").toBool());
    }
    void windDirectionChartRendersArrows_data()
    {
        QTest::addColumn<QString>("renderer");
        QTest::newRow("cpu") << QString("cpu");
        QTest::newRow("gpu") << QString("gpu");
    }
    void windDirectionChartRendersArrows()
    {
        QFETCH(QString, renderer);
        WeatherModel model;
        model.setAutoRefresh(false);
        model.refresh(false);
        QVERIFY(model.hasData());
        QVariantMap definition;
        for (const auto &entry : model.chartGroups()) if (entry.toMap()["key"] == "windDirection") definition = entry.toMap();
        QVERIFY(!definition.isEmpty());
        QQmlEngine engine;
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        const auto originalRenderer = preferences->renderer();
        const auto restore = qScopeGuard([&] { preferences->setRenderer(originalRenderer); });
        preferences->setRenderer(renderer);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import QtQuick.Layouts
            import pl.meteo.desktop
            Window {
                id: root
                required property WeatherModel weather
                required property var directionChart
                required property var windChart
                width: 650; height: 600; visible: true
                ColumnLayout {
                    anchors.fill: parent
                    Meteogram {
                        Layout.fillWidth: true; weather: root.weather
                        title: "Kierunek wiatru"; groupKey: "windDirection"
                        fields: root.directionChart.fields
                        chartConfiguration: root.directionChart
                        chartHeight: 120
                    }
                    Meteogram {
                        Layout.fillWidth: true; weather: root.weather
                        title: "Prędkość wiatru"; groupKey: "wind"
                        fields: root.windChart.fields
                        chartConfiguration: root.windChart
                    }
                }
            }
        )", QUrl());
        QVariantMap wind;
        for (const auto &entry : model.chartGroups()) if (entry.toMap()["key"] == "wind") wind = entry.toMap();
        QScopedPointer<QObject> chart(component.createWithInitialProperties({{"weather", QVariant::fromValue(&model)},
            {"directionChart", definition}, {"windChart", wind}}));
        QVERIFY2(chart, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(chart.data());
        QVERIFY(window);
        QTest::qWait(150);
        auto *gpu = chart->findChild<GpuPlot *>("gpu-base-windDirection");
        if (qEnvironmentVariableIsSet("METEO_REQUIRE_GPU") && renderer == "gpu") QVERIFY(gpu);
        auto *plot = chart->findChild<QQuickItem *>("plot-windDirection");
        QVERIFY(plot);
        QVERIFY(plot->height() > 0);
        const auto image = window->grabWindow();
        if (const auto path = qEnvironmentVariable("METEO_WIND_SCREENSHOT"); !path.isEmpty()) QVERIFY(image.save(path));
        int blue = 0;
        const auto origin = plot->mapToScene({0,0}).toPoint();
        for (int y = origin.y(); y < std::min(image.height(), origin.y()+int(plot->height())); ++y)
            for (int x = 50; x < 606; ++x) {
                const auto color = image.pixelColor(x,y);
                if (color.blue() > 90 && color.red() < 90 && color.green() < 90) ++blue;
            }
        QVERIFY2(blue > 50, qPrintable(QString("Expected visible wind arrows; found %1 blue pixels, plot %2 x %3").arg(blue).arg(plot->width()).arg(plot->height())));
        auto *speedChart = chart->findChild<QObject *>("chart-wind");
        auto *speedPlot = chart->findChild<QQuickItem *>("plot-wind");
        QVERIFY(speedChart); QVERIFY(speedPlot);
        QCOMPARE(speedChart->property("directionBandHeight").toInt(), 30);
        const auto speedOrigin = speedPlot->mapToScene({0,0}).toPoint();
        const int zero = speedOrigin.y() + qRound(speedPlot->height() - speedChart->property("bottomMargin").toDouble());
        int arrowsBelowZero = 0;
        for (int y=zero+2; y<zero+28 && y<image.height(); ++y) for (int x=50; x<606; ++x) {
            const auto color = image.pixelColor(x,y);
            if (color.blue()>90 && color.red()<90 && color.green()<90) ++arrowsBelowZero;
        }
        QVERIFY2(arrowsBelowZero>50, "The integrated wind chart must render arrows below the 0 m/s baseline");
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
            auto *settings = object->findChild<QObject *>("meteoSettingsDialog");
            QVERIFY(settings);
            settings->setProperty("section",1);
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
        auto *navigation=findItem(findItem,item,"forecastNavigation");
        QVERIFY(navigation);
        auto *tabGroup=navigation->property("tabGroup").value<QObject *>();
        QVERIFY(tabGroup);
        auto *selectedTab=tabGroup->property("checkedButton").value<QObject *>();
        QVERIFY(selectedTab);
        QCOMPARE(selectedTab->property("page").toInt(),0);
        auto *header=findItem(findItem,item,"forecastHeader");
        QVERIFY(header);
        QVERIFY(header->height()<100);
        auto *status=findItem(findItem,item,"chartStatusBar");
        QVERIFY(status);
        QVERIFY(status->mapToScene({0,0}).y()>plot->mapToScene({0,0}).y()+plot->height());
        auto *zoomSlider=findItem(findItem,item,"chartZoomSlider");
        QVERIFY(zoomSlider);
        QCOMPARE(zoomSlider->property("value").toDouble(),1.0);
        QVERIFY(!findItem(findItem,item,"viewOptionsButton"));
        auto *settings=object->findChild<QObject *>("meteoSettingsDialog");
        QVERIFY(settings);
        settings->setProperty("section",0);
        QVERIFY(QMetaObject::invokeMethod(object.data(),"openSettings"));
        QTRY_VERIFY(settings->property("visible").toBool());
        auto *timeRange=settings->findChild<QObject *>("settingsTimeRange");
        QVERIFY(timeRange);
        QVERIFY(QMetaObject::invokeMethod(timeRange,"activated",Q_ARG(int,1)));
        QCOMPARE(object->property("viewportEnd").toDouble()-object->property("viewportStart").toDouble(),24*3600.0);
        QVERIFY(QMetaObject::invokeMethod(object.data(),"resetRange"));
        auto *about = settings->findChild<QObject *>("meteoAboutDialog");
        QVERIFY(about);
        QVERIFY(QMetaObject::invokeMethod(about, "open"));
        QTRY_VERIFY(about->property("visible").toBool());
        auto *aboutPage = about->findChild<QObject *>("meteoAboutPage");
        QVERIFY(aboutPage);
        auto *authorEmail = about->findChild<QObject *>("authorEmail");
        QVERIFY(authorEmail);
        QVERIFY(authorEmail->property("text").toString().contains("mailto:kapiwko@gmail.com"));
        auto *sourceInfo = about->findChild<QObject *>("forecastSourceInfo");
        QVERIFY(sourceInfo);
        QVERIFY(sourceInfo->property("text").toString().contains(model.runText()));
        if (const auto path = qEnvironmentVariable("METEO_ABOUT_SCREENSHOT"); !path.isEmpty()) {
            QTest::qWait(400);
            QVERIFY(window.grabWindow().save(path));
        }
        QVERIFY(QMetaObject::invokeMethod(about, "close"));
        QTRY_VERIFY(!about->property("visible").toBool());
        auto *settingsSections = settings->findChild<QObject *>("settingsSections");
        QVERIFY(settingsSections);
        QCOMPARE(settingsSections->property("count").toInt(), 4);
        auto *profileSettings = settings->findChild<QQuickItem *>("meteogramDisplaySettings");
        auto *readingSettings = settings->findChild<QQuickItem *>("readingSettings");
        auto *applicationSettings = settings->findChild<QQuickItem *>("applicationSettings");
        QVERIFY(profileSettings); QVERIFY(readingSettings); QVERIFY(applicationSettings);
        settings->setProperty("section", 1);
        QTRY_VERIFY(profileSettings->isVisible());
        QVERIFY(!readingSettings->isVisible()); QVERIFY(!applicationSettings->isVisible());
        settings->setProperty("section", 2);
        QTRY_VERIFY(readingSettings->isVisible());
        QVERIFY(!profileSettings->isVisible()); QVERIFY(!applicationSettings->isVisible());
        QVERIFY(readingSettings->findChild<QObject *>("probeModeSetting"));
        settings->setProperty("section", 3);
        QTRY_VERIFY(applicationSettings->isVisible());
        QVERIFY(!profileSettings->isVisible()); QVERIFY(!readingSettings->isVisible());
        auto *settingsPath = applicationSettings->findChild<QQuickItem *>("settingsFilePath");
        QVERIFY(settingsPath);
        QTRY_VERIFY(settingsPath->width() > 200 && settingsPath->height() < 100);
        item->setWidth(450);
        window.resize(450, 1000);
        QTRY_VERIFY(settingsPath->width() > 200 && settingsPath->height() < 100);
        QTRY_VERIFY(settingsPath->mapToScene({settingsPath->width(), 0}).x() <= window.width());
        item->setWidth(780);
        window.resize(780, 1000);
        QVERIFY(QMetaObject::invokeMethod(settings,"close"));
        QTRY_VERIFY(!settings->property("visible").toBool());
        auto *chart = plot->parentItem();
        const auto originalBounds = chart->property("bounds").toMap();
        QVERIFY(originalBounds.contains("high"));
        const QStringList seriesToToggle{"airtmp_point", "airtmp_max", "dwptmp_point", "wchill_point", "wchill_max", "grdtmp_min", "grdtmp_max"};
        for (const auto &key : seriesToToggle)
            QVERIFY(QMetaObject::invokeMethod(chart, "toggle", Q_ARG(QVariant, key)));
        QCOMPARE(chart->property("bounds").toMap(), originalBounds);
        for (const auto &key : seriesToToggle)
            QVERIFY(QMetaObject::invokeMethod(chart, "toggle", Q_ARG(QVariant, key)));
        QTest::qWait(50);
        auto *gpu = qobject_cast<GpuPlot *>(findItem(findItem, item, "gpu-base-temperature"));
        const auto baseFrames = gpu ? gpu->frameCount() : 0;
        const auto cursorPoint = plot->mapToScene(QPointF(plot->width() / 2, plot->height() / 2)).toPoint();
        QVERIFY(!object->property("identificationEnabled").toBool());
        auto *identification = findItem(findItem, item, "identificationButton");
        QVERIFY(identification);
        QTest::mouseMove(&window, cursorPoint);
        QTest::qWait(250);
        QCOMPARE(object->property("selectedTime").toDouble(), 0.0);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, cursorPoint);
        QVERIFY(!object->property("pinned").toBool());
        const auto toggleIdentification = [&] {
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                identification->mapToScene({identification->width()/2, identification->height()/2}).toPoint());
        };
        toggleIdentification();
        QVERIFY(object->property("identificationEnabled").toBool());
        QTest::mouseMove(&window, cursorPoint);
        QTRY_VERIFY(object->property("selectedTime").toDouble() > 0);
        auto *inspector = object->findChild<QObject *>("forecastInspector");
        QVERIFY(inspector);
        QTRY_VERIFY(inspector->property("visible").toBool());
        const auto detailList = [](QObject *popup, const char *property) {
            const auto value = popup->property(property);
            return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toList() : value.toList();
        };
        QVERIFY(!detailList(inspector, "details").isEmpty());
        for (const auto &value : detailList(inspector, "details"))
            QVERIFY((QStringList{"temperature", "ground"}.contains(value.toMap()["group"].toString())));
        if (gpu) {
            QTest::qWait(50);
            QCOMPARE(gpu->frameCount(), baseFrames); // Moving the cursor must not rebuild the chart geometry.
        }
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, cursorPoint);
        QVERIFY(object->property("pinned").toBool());
        QTest::keyClick(&window, Qt::Key_Escape);
        QTRY_VERIFY(!inspector->property("visible").toBool());
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        QVERIFY(preferences);
        const auto previousProbe = preferences->probeMode();
        const auto restoreProbe = qScopeGuard([&] { preferences->setProbeMode(previousProbe); });
        toggleIdentification();
        QVERIFY(!object->property("identificationEnabled").toBool());
        QTest::mouseMove(&window, {10, 10});
        QTest::mouseMove(&window, cursorPoint);
        QTest::qWait(250);
        QVERIFY(!inspector->property("visible").toBool());
        QCOMPARE(object->property("selectedTime").toDouble(), 0.0);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, cursorPoint);
        QVERIFY(!inspector->property("visible").toBool());
        QVERIFY(!object->property("pinned").toBool());
        toggleIdentification();
        QVERIFY(object->property("identificationEnabled").toBool());
        preferences->setProbeMode("all");
        item->setWidth(1100);
        window.resize(1100, 1000);
        QTest::mouseMove(&window, {10, 10});
        QTest::mouseMove(&window, plot->mapToScene({plot->width()/2, plot->height()/2}).toPoint());
        QTRY_VERIFY(inspector->property("visible").toBool());
        QTRY_COMPARE(inspector->property("columns").toInt(), 2);
        QVERIFY(detailList(inspector, "details").size() > 9);
        if (const auto path = qEnvironmentVariable("METEO_INSPECTOR_SCREENSHOT"); !path.isEmpty()) {
            QTest::qWait(400);
            QVERIFY(window.grabWindow().save(path));
        }
        inspector->setProperty("category", "wind");
        QVERIFY(!detailList(inspector, "visibleDetails").isEmpty());
        for (const auto &value : detailList(inspector, "visibleDetails")) QCOMPARE(value.toMap()["group"].toString(), "wind");
        object->setProperty("pinned", true);
        item->setWidth(450);
        window.resize(450, 1000);
        QTRY_COMPARE(inspector->property("columns").toInt(), 1);
        inspector->setProperty("category", "");
        object->setProperty("identificationEnabled", false);
        QTRY_VERIFY(!inspector->property("visible").toBool());
        QVERIFY(!object->property("pinned").toBool());
        preferences->setProbeMode("chart");
        item->setWidth(780);
        window.resize(780, 1000);
        const auto fullSpan = object->property("viewportEnd").toDouble() - object->property("viewportStart").toDouble();
        QVERIFY(QMetaObject::invokeMethod(object.data(), "zoom", Q_ARG(QVariant, 0.5), Q_ARG(QVariant, model.firstTimestamp() + fullSpan / 2)));
        QVERIFY(object->property("viewportEnd").toDouble() - object->property("viewportStart").toDouble() < fullSpan);
        for (int i=0;i<5;++i) QVERIFY(QMetaObject::invokeMethod(object.data(), "zoom", Q_ARG(QVariant,0.1), Q_ARG(QVariant,model.firstTimestamp()+fullSpan/2)));
        QCOMPARE(object->property("viewportEnd").toDouble()-object->property("viewportStart").toDouble(),24*3600.0);
        QTRY_COMPARE(zoomSlider->property("value").toDouble(),fullSpan/(24*3600));
        item->setWidth(450);
        window.resize(450,1000);
        QTest::qWait(50);
        QVERIFY(zoomSlider->mapToScene({zoomSlider->width(),0}).x()<=450);
        item->setWidth(780);
        window.resize(780,1000);
        object->setProperty("viewMode",2);
        QTest::qWait(100);
        QList<QQuickItem *> cards;
        const auto collect=[&](auto &&self,QQuickItem *parent)->void {
            if (parent->objectName()=="forecastDayCard") cards.append(parent);
            for (auto *child:parent->childItems()) self(self,child);
        };
        collect(collect,item);
        QVERIFY(cards.size()>1);
        for (auto *card:cards) QVERIFY(qAbs(card->width()-cards.first()->width())<1);
    }
    void chartContextMenuSharesLegendStateAndSaveAction_data()
    {
        QTest::addColumn<QString>("group");
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("partner");
        QTest::newRow("temperature range") << QStringLiteral("temperature") << QStringLiteral("airtmp_max") << QStringLiteral("airtmp_min");
        QTest::newRow("storms and lightning") << QStringLiteral("probability") << QStringLiteral("storm_max") << QStringLiteral("flash_max");
    }
    void chartContextMenuSharesLegendStateAndSaveAction()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QFETCH(QString, group); QFETCH(QString, key); QFETCH(QString, partner);
        WeatherModel weather; weather.setAutoRefresh(false); weather.refresh(false);
        QVERIFY(weather.hasData());
        QVariantMap definition;
        for (const auto &entry : weather.chartGroups()) if (entry.toMap()["key"] == group) definition = entry.toMap();
        QVERIFY(!definition.isEmpty());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import pl.meteo.desktop
            Meteogram {
                property Item requestedChart: null
                exportHandler: chart => requestedChart = chart
                legendOverride: 1
                identificationEnabled: true
            }
        )", QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"weather", QVariant::fromValue(&weather)},
            {"title", definition["title"]}, {"groupKey", group}, {"fields", definition["fields"]}, {"chartConfiguration", definition}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *chart = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window; window.resize(780, 400);
        chart->setParentItem(window.contentItem()); chart->setSize({780, 400}); window.show();
        auto *plot = chart->findChild<QQuickItem *>("plot-" + group);
        auto *menu = chart->findChild<QObject *>("chartContextMenu");
        auto *layer = chart->findChild<QQuickItem *>("chartLayer-" + key);
        const auto findVisual = [](auto &&self, QQuickItem *item, const QString &name) -> QQuickItem * {
            if (item->objectName() == name) return item;
            for (auto *child : item->childItems()) if (auto *found = self(self, child, name)) return found;
            return nullptr;
        };
        auto *legend = findVisual(findVisual, chart, "chartLegend-" + key);
        auto *save = chart->findChild<QQuickItem *>("saveChartMenuItem");
        QVERIFY(plot); QVERIFY(menu); QVERIFY(layer); QVERIFY(legend); QVERIFY(save);
        const auto bounds = chart->property("bounds").toMap(), rightBounds = chart->property("rightBounds").toMap();
        QSignalSpy pins(chart, SIGNAL(pinRequested(double,QPointF)));
        QVERIFY(pins.isValid());
        const auto click = [](QQuickItem *item, Qt::MouseButton button) {
            QTest::mouseClick(item->window(), button, Qt::NoModifier,
                item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        };
        QTest::qWait(100);
        click(plot, Qt::RightButton);
        QTRY_VERIFY(menu->property("opened").toBool());
        QCOMPARE(pins.count(), 0);
        QCOMPARE(layer->property("text"), legend->property("text"));
        QVERIFY(layer->property("checked").toBool());
        click(layer, Qt::LeftButton);
        QTRY_VERIFY(!menu->property("visible").toBool());
        QVERIFY(!legend->property("checked").toBool());
        const auto hidden = chart->property("hiddenKeys").value<QJSValue>().toVariant().toList();
        QVERIFY(hidden.contains(key)); QVERIFY(hidden.contains(partner));
        QCOMPARE(chart->property("bounds").toMap(), bounds);
        QCOMPARE(chart->property("rightBounds").toMap(), rightBounds);
        click(legend, Qt::LeftButton);
        QVERIFY(legend->property("checked").toBool());
        QVERIFY(layer->property("checked").toBool());
        // Keyboard users can open the same native menu and save the very chart
        // they selected, without adding buttons beside every chart heading.
        plot->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Menu);
        QTRY_VERIFY(menu->property("opened").toBool());
        click(save, Qt::LeftButton);
        QTRY_VERIFY(!menu->property("visible").toBool());
        QCOMPARE(chart->property("requestedChart").value<QQuickItem *>(), chart);
        QCOMPARE(pins.count(), 0);
    }
    void standaloneChartSaveUsesCurrentRangeAndVisibility()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        const auto nativeDialogsDisabled = QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        const auto restore = qScopeGuard([&] { QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, nativeDialogsDisabled); });
        WeatherModel weather; weather.setAutoRefresh(false); weather.refresh(false);
        QVERIFY(weather.hasData());
        const auto definition = weather.chartGroups().first().toMap();
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "Meteogram");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"weather", QVariant::fromValue(&weather)},
            {"title", definition["title"]}, {"groupKey", definition["key"]}, {"fields", definition["fields"]}, {"chartConfiguration", definition}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *chart = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window; window.resize(780, 350);
        chart->setParentItem(window.contentItem()); chart->setSize({780, 350}); window.show();
        chart->setProperty("endTime", weather.firstTimestamp() + 12 * 3600);
        QVERIFY(QMetaObject::invokeMethod(chart, "toggle", Q_ARG(QVariant, "airtmp_point")));
        const auto hidden = chart->property("hiddenKeys").value<QJSValue>().toVariant();
        QTest::qWait(100);
        QVERIFY(QMetaObject::invokeMethod(chart, "saveChart"));
        auto *dialog = chart->property("saveDialog").value<QObject *>();
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        auto *exporter = dialog->findChild<ChartExporter *>();
        QVERIFY(exporter);
        QSignalSpy saved(exporter, &ChartExporter::saved), failed(exporter, &ChartExporter::failed);
        QTemporaryDir files; QVERIFY(files.isValid());
        const auto file = QUrl::fromLocalFile(files.filePath("chart.png"));
        // Emulate the native picker returning a file; its filename editor and
        // platform-specific validation are outside this component's contract.
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QVERIFY(dialog->setProperty("selectedFile", file));
        QVERIFY(QMetaObject::invokeMethod(dialog, "accepted"));
        QTRY_COMPARE_WITH_TIMEOUT(saved.count() + failed.count(), 1, 5000);
        QVERIFY2(failed.isEmpty(), qPrintable(exporter->error()));
        const QImage image(file.toLocalFile());
        QVERIFY(!image.isNull());
        QVERIFY(image.text("Title").contains(weather.locationName()));
        QVERIFY(image.text("Description").contains(weather.formatTime(weather.firstTimestamp() + 12 * 3600, "d MMM yyyy HH:mm")));
        QVERIFY(image.text("Source").contains("meteo.pl"));
        QCOMPARE(chart->property("hiddenKeys").value<QJSValue>().toVariant(), hidden);
        QVERIFY(!chart->property("exportMode").toBool());
    }
    void exportsCompleteChartsAndHonorsVisibleSeries_data()
    {
        QTest::addColumn<QString>("renderer");
        QTest::newRow("cpu") << QStringLiteral("cpu");
        QTest::newRow("gpu") << QStringLiteral("gpu");
    }
    void exportsCompleteChartsAndHonorsVisibleSeries()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QFETCH(QString,renderer);
        UserPreferences preferences;
        const auto previous=preferences.renderer();
        preferences.setRenderer(renderer);
        const auto restore=qScopeGuard([&]{ preferences.setRenderer(previous); });
        WeatherModel model; model.setAutoRefresh(false); model.refresh(false);
        QVERIFY(model.hasData());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop","ForecastPanel");
        QVERIFY2(component.isReady(),qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"weather",QVariant::fromValue(&model)}}));
        QVERIFY(object);
        auto *item=qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;window.resize(720,450);
        item->setParentItem(window.contentItem());item->setSize({720,450});window.show();
        QTest::qWait(100);
        QVariantList charts;
        const auto collect=[&](auto &&self,QQuickItem *parent)->void {
            if(parent->objectName().startsWith("chart-") && parent->property("exportMode").isValid()) charts.append(QVariant::fromValue(parent));
            for(auto *child:parent->childItems()) self(self,child);
        };
        collect(collect,item);
        QCOMPARE(charts.size(),7);
        QVERIFY(QMetaObject::invokeMethod(object.data(),"chooseExport"));
        auto *selection=object->findChild<QObject *>("chartExportSelection");
        QVERIFY(selection);
        QTRY_VERIFY(selection->property("opened").toBool());
        QCOMPARE(object->property("exportSelectedKeys").value<QJSValue>().toVariant().toList().size(),7);
        object->setProperty("exportSelectedKeys",QVariantList{QStringLiteral("temperature")});
        QVERIFY(QMetaObject::invokeMethod(selection,"close"));
        auto *exporter=object->findChild<ChartExporter *>("chartExporter");
        QVERIFY(exporter);
        QSignalSpy saved(exporter,&ChartExporter::saved),failed(exporter,&ChartExporter::failed);
        QTemporaryDir files;QVERIFY(files.isValid());
        const auto exportImage=[&](const QString &name,const QVariantList &items,const QString &title) {
            QVariant result;
            const auto file=QUrl::fromLocalFile(files.filePath(name));
            const auto invoked=QMetaObject::invokeMethod(object.data(),"exportToFile",Q_RETURN_ARG(QVariant,result),
                Q_ARG(QVariant,file),Q_ARG(QVariant,QVariant(items)),Q_ARG(QVariant,title));
            return invoked && result.toBool();
        };
        QVERIFY(exportImage("all.png",charts,"Meteogram"));
        QTRY_COMPARE_WITH_TIMEOUT(saved.count()+failed.count(),1,5000);
        QCOMPARE(failed.count(),0);
        const QImage image(files.filePath("all.png"));
        QVERIFY(!image.isNull());
        QCOMPARE(image.width(),qCeil(charts.first().value<QQuickItem *>()->width())+32);
        QVERIFY(image.height()>window.height()*3);
        QCOMPARE(image.text("Title"),QStringLiteral("Warszawa · Meteogram"));
        QVERIFY(image.text("Source").contains("meteo.pl"));
        int colored=0;
        for(int y=image.height()-200;y<image.height()-70;++y) for(int x=70;x<image.width()-70;++x) {
            const auto pixel=image.pixelColor(x,y);
            if(qAbs(pixel.red()-pixel.blue())>40) ++colored;
        }
        QVERIFY2(colored>100,"The final chart below the scroll viewport must be rendered");
        if(const auto path=qEnvironmentVariable("METEO_EXPORT_DIR");!path.isEmpty()) QVERIFY(image.save(QDir(path).filePath(renderer+".png")));
        auto *first=charts.first().value<QQuickItem *>();
        QVERIFY(exportImage("single.png",{charts.first()},"Temperatura"));
        QTRY_COMPARE_WITH_TIMEOUT(saved.count()+failed.count(),2,5000);
        QCOMPARE(failed.count(),0);
        const QImage single(files.filePath("single.png"));
        QVERIFY(!single.isNull());
        first->setProperty("legendOverride",0);
        QVERIFY(QMetaObject::invokeMethod(first,"toggle",Q_ARG(QVariant,"airtmp_point")));
        QTest::qWait(50);
        QVERIFY(exportImage("filtered.png",{charts.first()},"Temperatura"));
        QTRY_COMPARE_WITH_TIMEOUT(saved.count()+failed.count(),3,5000);
        QCOMPARE(failed.count(),0);
        const QImage filtered(files.filePath("filtered.png"));
        QVERIFY(!filtered.isNull());
        QVERIFY(filtered.height()<single.height());
        const auto reds=[](const QImage &picture) {
            int count=0;
            for(int y=0;y<picture.height();++y) for(int x=0;x<picture.width();++x) {
                const auto pixel=picture.pixelColor(x,y);
                if(pixel.red()>220 && pixel.green()<130 && pixel.blue()<150) ++count;
            }
            return count;
        };
        QVERIFY(reds(single)>100);
        QVERIFY(reds(filtered)<reds(single)/4);
        for(const auto &chart:charts) QVERIFY(!chart.value<QQuickItem *>()->property("exportMode").toBool());
    }
    void exportsFilteredReadingsAndRowsOutsideInspectorViewport()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        WeatherModel model; model.setAutoRefresh(false); model.refresh(false);
        QVERIFY(model.hasData());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "ForecastPanel");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"weather", QVariant::fromValue(&model)}}));
        QVERIFY(object);
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window; window.resize(540, 450);
        item->setParentItem(window.contentItem()); item->setSize({540, 450}); window.show();
        QTest::qWait(100);
        QVERIFY(QMetaObject::invokeMethod(object.data(), "showProbe", Q_ARG(QVariant, model.firstTimestamp()),
            Q_ARG(QVariant, QPointF(400, 100)), Q_ARG(QVariant, QString()), Q_ARG(QVariant, true)));
        object->setProperty("pinned", true);
        auto *inspector = object->findChild<QObject *>("forecastInspector");
        QVERIFY(inspector);
        QTRY_VERIFY(inspector->property("opened").toBool());
        inspector->setProperty("category", "wind");
        QTemporaryDir files;
        QVERIFY(files.isValid());
        const auto directory = qEnvironmentVariable("METEO_EXPORT_DIR", files.path());
        const auto csvPath = QDir(directory).filePath("meteo-reading-wind.csv");
        QVariant accepted;
        QVERIFY(QMetaObject::invokeMethod(object.data(), "exportReadingsToFile", Q_RETURN_ARG(QVariant, accepted),
            Q_ARG(QVariant, QUrl::fromLocalFile(csvPath)), Q_ARG(QVariant, QStringLiteral("csv"))));
        QVERIFY(accepted.toBool());
        QFile csv(csvPath); QVERIFY(csv.open(QIODevice::ReadOnly));
        const auto csvText = QString::fromUtf8(csv.readAll());
        QCOMPARE(csvText.count("\r\n"), 4); // Header plus the three wind parameters.
        QVERIFY(csvText.contains("wind10_sd_true_prev_point"));
        QVERIFY(!csvText.contains("airtmp_point"));
        QVERIFY(!object->property("exportFailed").toBool());
        auto *notice = object->findChild<QObject *>("exportNotice");
        QVERIFY(notice); QVERIFY(notice->property("visible").toBool());
        inspector->setProperty("category", "");
        auto *readings = inspector->findChild<QQuickItem *>("inspectorReadings");
        QVERIFY(readings);
        QTRY_VERIFY(readings->height() > window.height());
        auto *exporter = object->findChild<ChartExporter *>("chartExporter");
        QVERIFY(exporter);
        QSignalSpy saved(exporter, &ChartExporter::saved), failed(exporter, &ChartExporter::failed);
        const auto pngPath = QDir(directory).filePath("meteo-reading-all.png");
        QVERIFY(QMetaObject::invokeMethod(object.data(), "exportReadingsToFile", Q_RETURN_ARG(QVariant, accepted),
            Q_ARG(QVariant, QUrl::fromLocalFile(pngPath)), Q_ARG(QVariant, QStringLiteral("png"))));
        QVERIFY(accepted.toBool());
        QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 5000);
        QCOMPARE(failed.count(), 0);
        QImage image(pngPath); QVERIFY(!image.isNull());
        QVERIFY(image.height() > readings->height());
        QVERIFY(image.height() > window.height() + 150);
        QVERIFY(image.text("Title").contains("All"));
        const auto background = readings->property("color").value<QColor>();
        QCOMPARE(image.pixelColor(image.width()/2, image.height()/2), background);
        int finalReadingPixels = 0;
        for (int y = image.height()*3/4; y < image.height(); ++y)
            for (int x = 0; x < image.width()/2; ++x)
                if (image.pixelColor(x,y) == QColor("#f44336")) ++finalReadingPixels;
        QVERIFY(finalReadingPixels > 10); // The last parameter's swatch is below the scroll viewport.
        QVERIFY(object->property("pinned").toBool());
        QVERIFY(!object->property("exportingReadings").toBool());
        QVERIFY(object->property("exportMessage").toString().contains("reading"));
    }
    void exportCancelsWhenAChartChangesWithoutResizing()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            Item {
                width:200; height:100
                property bool exportMode:false
                property string exportRevision:"original range and series"
                Rectangle { anchors.fill:parent; color:"red" }
            }
        )",{});
        QScopedPointer<QObject> first(component.create()), second(component.create());
        QVERIFY(first); QVERIFY(second);
        QQuickWindow window; window.resize(220,220);
        qobject_cast<QQuickItem *>(first.data())->setParentItem(window.contentItem());
        qobject_cast<QQuickItem *>(second.data())->setParentItem(window.contentItem());
        window.show();
        QTemporaryDir directory;
        const auto path = directory.filePath("unchanged.png");
        QFile existing(path); QVERIFY(existing.open(QIODevice::WriteOnly)); existing.write("original"); existing.close();
        ChartExporter exporter;
        QSignalSpy failed(&exporter,&ChartExporter::failed), saved(&exporter,&ChartExporter::saved);
        const QVariantList items{QVariant::fromValue(first.data()),QVariant::fromValue(second.data())};
        QVERIFY(exporter.savePng(QUrl::fromLocalFile(path),items,{}));
        QTimer::singleShot(0,second.data(),[&] { second->setProperty("exportRevision","different range or hidden series"); });
        QTRY_COMPARE(failed.count(),1);
        QCOMPARE(saved.count(),0);
        QVERIFY(!exporter.busy());
        QVERIFY(!first->property("exportMode").toBool());
        QVERIFY(!second->property("exportMode").toBool());
        QVERIFY(existing.open(QIODevice::ReadOnly)); QCOMPARE(existing.readAll(),QByteArray("original"));
    }
    void exportFailurePreservesFileAndCanBeRetried()
    {
        QQuickWindow window;window.resize(200,200);window.show();
        QQuickItem chart(window.contentItem());chart.setSize({200,100});chart.setProperty("exportMode",false);
        QTemporaryDir directory;QVERIFY(directory.isValid());
        const auto path=directory.filePath("existing.png");
        QFile existing(path);QVERIFY(existing.open(QIODevice::WriteOnly));existing.write("original");existing.close();
        ChartExporter exporter;
        QSignalSpy failed(&exporter,&ChartExporter::failed),saved(&exporter,&ChartExporter::saved);
        const QVariantList items{QVariant::fromValue(&chart)};
        QVERIFY(exporter.savePng(QUrl::fromLocalFile(path),items,{{"title","Meteogram"}}));
        chart.setWidth(190);
        QCOMPARE(failed.count(),1);QVERIFY(!exporter.busy());QVERIFY(!chart.property("exportMode").toBool());
        QVERIFY(existing.open(QIODevice::ReadOnly));QCOMPARE(existing.readAll(),QByteArray("original"));existing.close();
        QVERIFY(exporter.savePng(QUrl::fromLocalFile(path),items,{{"title","Meteogram"}}));
        QTRY_COMPARE_WITH_TIMEOUT(saved.count(),1,5000);
        QCOMPARE(failed.count(),1);QVERIFY(!QImage(path).isNull());QVERIFY(!chart.property("exportMode").toBool());
        QVERIFY(!exporter.savePng(QUrl("https://example.org/output.png"),items,{}));
        QVERIFY(!exporter.busy());
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
            {"series", QVariantList{temperature, rain}}, {"height",220}}};
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
        QCOMPARE(chart->property("chartHeight").toInt(),220);
        chart->setProperty("legendOverride",0);
        QVERIFY(!chart->property("legendVisible").toBool());
        auto bounds = chart->property("bounds").toMap();
        auto right = chart->property("rightBounds").toMap();
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
        QCOMPARE(chart->property("bounds").toMap(), bounds);
        QCOMPARE(chart->property("rightBounds").toMap(), right);
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
            QTRY_VERIFY(chart->height() > closedHeight + 100);
            const auto openHeight = chart->height();
            object->setProperty("expandedSeries", "temperature/airtmp_point");
            QTRY_VERIFY(chart->height() > openHeight + 100);
            object->setProperty("expandedSeries", "");
            QTRY_VERIFY(qAbs(chart->height() - openHeight) < 2);
            object->setProperty("expandedChart", "");
            QTRY_VERIFY(qAbs(chart->height() - closedHeight) < 2);
            QTRY_VERIFY(qAbs(item->implicitHeight() - closedTotal) < 2);
        }
    }
    void savedPlaceSelectionSharesTheListAndKeepsIndependentViews()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QTemporaryDir settings;
        QVERIFY(settings.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        const auto restore = qScopeGuard([&] { QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_cache.path()+"/config"); });
        WeatherModel model, other;
        model.setAutoRefresh(false); other.setAutoRefresh(false);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "SavedPlaceSelection");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> selection(component.createWithInitialProperties({{"weather", QVariant::fromValue(&model)}}));
        QScopedPointer<QObject> otherSelection(component.createWithInitialProperties({{"weather", QVariant::fromValue(&other)}}));
        QVERIFY(selection); QVERIFY(otherSelection);
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        QVERIFY(preferences);
        QCOMPARE(preferences->favorites().size(), 1);
        const auto change = [](WeatherModel &weather, const QList<QVariant> &values) {
            weather.setLocationName(values[0].toString()); weather.setLatitude(values[1].toDouble()); weather.setLongitude(values[2].toDouble());
        };
        QSignalSpy changed(selection.data(), SIGNAL(locationSelected(QString,double,double)));
        QSignalSpy otherChanged(otherSelection.data(), SIGNAL(locationSelected(QString,double,double)));
        // Apply emitted selections as the application and each widget do in their own configuration.
        const auto apply = [&] {
            while (!changed.isEmpty()) change(model, changed.takeFirst());
            while (!otherChanged.isEmpty()) change(other, otherChanged.takeFirst());
        };
        preferences->savePlace("Kielce", 50.8661, 20.6286);
        QCOMPARE(preferences->favorites().size(), 2);
        QQmlComponent pickerComponent(&engine);
        pickerComponent.loadFromModule("pl.meteo.desktop", "PlacePicker");
        QVERIFY2(pickerComponent.isReady(), qPrintable(pickerComponent.errorString()));
        QScopedPointer<QObject> picker(pickerComponent.createWithInitialProperties({{"weather", QVariant::fromValue(&model)}}));
        QVERIFY(picker);
        QSignalSpy picked(picker.data(), SIGNAL(locationSelected(QString,double,double)));
        auto *combo = picker->findChild<QObject *>("headerLocationSelector");
        QVERIFY(combo);
        QCOMPARE(combo->property("currentIndex").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, 1)));
        QCOMPARE(picked.count(), 1);
        change(model, picked.takeFirst());
        QTRY_COMPARE(combo->property("currentIndex").toInt(), 1);
        QCOMPARE(other.locationName(), "Warszawa");
        preferences->removePlace(50.8661, 20.6286);
        QTRY_VERIFY(!changed.isEmpty()); apply();
        QTRY_COMPARE(model.locationName(), "Warszawa");
        preferences->removePlace(52.2297, 21.0122);
        QTRY_VERIFY(!changed.isEmpty() && !otherChanged.isEmpty()); apply();
        QTRY_VERIFY(model.locationName().isEmpty() && other.locationName().isEmpty());
        QCOMPARE(preferences->favorites().size(), 0);
        preferences->savePlace("Gdańsk", 54.352, 18.6466);
        QTRY_VERIFY(!changed.isEmpty() && !otherChanged.isEmpty()); apply();
        QTRY_COMPARE(model.locationName(), "Gdańsk");
        QTRY_COMPARE(other.locationName(), "Gdańsk");
        preferences->rememberPlace("Gdańsk", 54.352, 18.6466);
        QCOMPARE(preferences->favorites().size(), 1);
    }
    void profileButtonsDuplicateRenameAndDeleteWithoutPreparingAName()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "DisplaySettings");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.create());
        QVERIFY(object);
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        const auto original = preferences->profile();
        const auto restore = qScopeGuard([&] { preferences->setProfile(original); });
        preferences->setProfile("classic");
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window; window.resize(700, 800);
        item->setParentItem(window.contentItem()); item->setWidth(680); window.show();
        auto *duplicate = object->findChild<QQuickItem *>("profileDuplicateButton");
        auto *remove = object->findChild<QQuickItem *>("profileDeleteButton");
        auto *name = object->findChild<QQuickItem *>("profileNameField");
        QVERIFY(duplicate); QVERIFY(remove); QVERIFY(name);
        QTRY_VERIFY(duplicate->width() > 0);
        QVERIFY(duplicate->isEnabled()); QVERIFY(!remove->isEnabled());
        const auto click = [&](QQuickItem *control) {
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                control->mapToScene({control->width()/2, control->height()/2}).toPoint());
        };
        const auto count = preferences->profiles().size();
        click(duplicate);
        QTRY_VERIFY(preferences->profile().startsWith("user:"));
        QCOMPARE(preferences->profiles().size(), count + 1);
        QVERIFY(remove->isEnabled()); QVERIFY(!name->property("readOnly").toBool());
        name->setProperty("text", "Profil testowy");
        QVERIFY(QMetaObject::invokeMethod(name, "editingFinished"));
        QCOMPARE(preferences->profiles().last().toMap()["name"].toString(), "Profil testowy");
        click(remove);
        QTRY_COMPARE(preferences->profile(), "classic");
        QCOMPARE(preferences->profiles().size(), count);
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
    void sunSummaryUsesTheCurrentForecastDayAndSelectedTimeZone()
    {
        const auto hash = QCryptographicHash::hash("um4_60:52.229700:21.012200", QCryptographicHash::Sha256).toHex();
        const auto path = m_cache.path() + "/meteo-pl/" + hash + ".json";
        QFile input(path); QVERIFY(input.open(QIODevice::ReadOnly));
        const auto original = input.readAll(); input.close();
        const auto restore = qScopeGuard([&] { QFile file(path); if (file.open(QIODevice::WriteOnly)) file.write(original); });
        auto envelope = QJsonDocument::fromJson(original).object();
        auto payload = QJsonDocument::fromJson(envelope["payload"].toString().toUtf8()).object();
        auto fields = payload["data"].toObject();
        const auto first = QDateTime::currentSecsSinceEpoch() - 25*3600;
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            auto field = it.value().toObject();
            field["first_timestamp"] = QString::number(first);
            it.value() = field;
        }
        payload["data"] = fields;
        envelope["payload"] = QString::fromUtf8(QJsonDocument(payload).toJson());
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(envelope).toJson()); file.close();
        WeatherModel model; model.setAutoRefresh(false); model.refresh(false);
        QVERIFY(model.hasData());
        QVERIFY(QDateTime::fromSecsSinceEpoch(first).date() < QDate::currentDate());
        auto solar = model.solarTimes();
        QVERIFY(solar["today"].toBool());
        QCOMPARE(solar["date"].toString(), QDate::currentDate().toString(Qt::ISODate));
        QVERIFY(solar["sunrise"].toLongLong() > 0);
        QVERIFY(solar["sunset"].toLongLong() > solar["sunrise"].toLongLong());
        QCOMPARE(solar["sunriseText"].toString(), model.formatTime(solar["sunrise"].toDouble(), "HH:mm"));
        QCOMPARE(solar["sunsetText"].toString(), model.formatTime(solar["sunset"].toDouble(), "HH:mm"));
        model.setUtc(true);
        solar = model.solarTimes();
        QCOMPARE(solar["date"].toString(), QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate));
        QCOMPARE(solar["sunriseText"].toString(), model.formatTime(solar["sunrise"].toDouble(), "HH:mm"));
        QCOMPARE(solar["sunsetText"].toString(), model.formatTime(solar["sunset"].toDouble(), "HH:mm"));
        QVERIFY(solar["status"].toString().isEmpty());
    }
    void nativeSamplingIsSharedByReadingsCursorAndCsv()
    {
        const auto hash = QCryptographicHash::hash("um4_60:52.229700:21.012200", QCryptographicHash::Sha256).toHex();
        const auto path = m_cache.path() + "/meteo-pl/" + hash + ".json";
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll(); file.close();
        const auto restore = qScopeGuard([&] { QFile output(path); if (output.open(QIODevice::WriteOnly)) output.write(original); });
        auto envelope = QJsonDocument::fromJson(original).object();
        auto payload = QJsonDocument::fromJson(envelope["payload"].toString().toUtf8()).object();
        auto fields = payload["data"].toObject();
        const auto first = fields["airtmp_point"].toObject()["first_timestamp"].toString().toLongLong();
        for (const auto &key : {"dwptmp_point", "pcpttl_aver", "pcpttl_type_max"}) {
            auto field = fields["airtmp_point"].toObject();
            field["interval"] = 10800;
            field["data"] = QString(key) == "pcpttl_type_max" ? QJsonArray{2,3,0} : QJsonArray{1,9,QJsonValue()};
            fields[key] = field;
        }
        payload["data"] = fields;
        envelope["payload"] = QString::fromUtf8(QJsonDocument(payload).toJson());
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(envelope).toJson()); file.close();
        WeatherModel model; model.setAutoRefresh(false); model.refresh(false);
        QVERIFY(model.hasData());
        const auto selected = first + 7200;
        const auto rows = model.detailsAt(selected);
        const auto rowFor = [&](const QString &key) {
            for (const auto &value : rows) if (value.toMap()["key"] == key) return value.toMap();
            return QVariantMap();
        };
        const auto instant = rowFor("dwptmp_point"), rain = rowFor("pcpttl_aver");
        QCOMPARE(instant["number"].toDouble(),9.0);
        QCOMPARE(instant["sourceTimestamp"].toLongLong(),first+10800);
        QVERIFY(!instant["intervalTo"].isValid());
        QCOMPARE(rain["number"].toDouble(),1.0);
        QCOMPARE(rain["sourceTimestamp"].toLongLong(),first);
        QCOMPARE(rain["intervalFrom"].toLongLong(),first);
        QCOMPARE(rain["intervalTo"].toLongLong(),first+10800);
        QCOMPARE(rain["sampling"].toString(),"intervalTotal");
        const auto bars = model.series()["pcpttl_aver"].toList();
        QCOMPARE(bars[0].toMap()["precipitationType"].toDouble(),2.0);
        QCOMPARE(bars[1].toMap()["precipitationType"].toDouble(),3.0);
        QQmlEngine engine;
        QQmlComponent component(&engine,QUrl("qrc:/qt/qml/pl/meteo/desktop/Meteogram.qml"));
        const auto chart = model.chartGroups().first().toMap();
        QScopedPointer<QObject> item(component.createWithInitialProperties({{"weather",QVariant::fromValue(&model)},
            {"title","Test"},{"groupKey","temperature"},{"fields",chart["fields"]},{"selectedTime",double(selected)}}));
        QVERIFY2(item,qPrintable(component.errorString()));
        const auto samples = item->property("probeSamples").value<QJSValue>().toVariant().toMap();
        QCOMPARE(samples["dwptmp_point"].toMap(),instant);
        QCOMPARE(samples["pcpttl_aver"].toMap(),rain);
        ReadingsExporter exporter;
        QTemporaryDir output;
        const auto csv = exporter.saveCsv(QUrl::fromLocalFile(output.filePath("samples.csv")),rows,
            {{"timestamp",double(selected)},{"utc",true}});
        QVERIFY(csv["ok"].toBool());
        QFile data(csv["path"].toString()); QVERIFY(data.open(QIODevice::ReadOnly));
        const auto text = QString::fromUtf8(data.readAll());
        const auto iso = [](qint64 time) { return QDateTime::fromSecsSinceEpoch(time,QTimeZone::UTC).toString(Qt::ISODate); };
        QVERIFY(text.contains('"'+iso(first+10800)+"\";\"\";\"\";\"instant\""));
        QVERIFY(text.contains('"'+iso(first)+"\";\""+iso(first)+"\";\""+iso(first+10800)+"\";\"intervalTotal\""));
    }
    void clockUpdatesKeepChartDataStableAndUnitsDoNotRewriteTheCache()
    {
        WeatherModel model;
        model.setAutoRefresh(false); model.refresh(false);
        QVERIFY(model.hasData());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQml
            import pl.meteo.desktop
            QtObject {
                required property WeatherModel weather
                property int updates: 0
                property var points: weather.series
                onPointsChanged: updates++
            }
        )",{});
        QScopedPointer<QObject> binding(component.createWithInitialProperties({{"weather",QVariant::fromValue(&model)}}));
        QVERIFY2(binding,qPrintable(component.errorString()));
        const auto updates = binding->property("updates").toInt();
        const auto revision = model.dataRevision();
        QSignalSpy changed(&model,&WeatherModel::dataChanged);
        for (int i=0;i<10;++i) QVERIFY(QMetaObject::invokeMethod(&model,"updateCurrentTime"));
        QCOMPARE(changed.count(),0);
        QCOMPARE(model.dataRevision(),revision);
        QCOMPARE(binding->property("updates").toInt(),updates);
        const auto hash = QCryptographicHash::hash("um4_60:52.229700:21.012200",QCryptographicHash::Sha256).toHex();
        const auto path = m_cache.path()+"/meteo-pl/"+hash+".json";
        const auto modified = QFileInfo(path).lastModified();
        model.setWindUnit("km/h");
        QCOMPARE(changed.count(),1);
        QVERIFY(model.dataRevision()>revision);
        QVERIFY(binding->property("updates").toInt()>updates);
        QCOMPARE(QFileInfo(path).lastModified(),modified);
        QVERIFY(!model.busy());
        QVERIFY(model.usingCache());
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
    void locationEditorAcceptsPreciseCoordinatesAndValidatesManualDrafts()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl("qrc:/qt/qml/pl/meteo/desktop/LocationEditor.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const double latitude = 50.866123456789012;
        const double longitude = 20.628612345678901;
        QScopedPointer<QObject> editor(component.createWithInitialProperties({
            {"locationName", "GPS point"}, {"latitude", latitude}, {"longitude", longitude}}));
        QVERIFY2(editor, qPrintable(component.errorString()));
        QVERIFY(editor->property("valid").toBool());
        auto *manual = editor->findChild<QObject *>("manualLocationToggle");
        auto *lat = editor->findChild<QObject *>("manualLatitude");
        auto *lon = editor->findChild<QObject *>("manualLongitude");
        QVERIFY(manual); QVERIFY(lat); QVERIFY(lon);
        manual->setProperty("checked", true);
        QVERIFY(editor->property("valid").toBool());
        QCOMPARE(lat->property("text").toString(), "50.866123");
        QCOMPARE(lon->property("text").toString(), "20.628612");
        QCOMPARE(editor->property("latitude").toDouble(), latitude);
        QCOMPARE(editor->property("longitude").toDouble(), longitude);
        lat->setProperty("text", "-");
        QVERIFY(!editor->property("valid").toBool());
        manual->setProperty("checked", false);
        QVERIFY(editor->property("valid").toBool());
        manual->setProperty("checked", true);
        lat->setProperty("text", "50,123456");
        QVERIFY(QMetaObject::invokeMethod(lat, "textEdited"));
        QCOMPARE(editor->property("latitude").toDouble(), 50.123456);
        QVERIFY(editor->property("valid").toBool());
        editor->setProperty("model", "unknown");
        QVERIFY(!editor->property("valid").toBool());
        editor->setProperty("latitude", 1.2345678912345);
        editor->setProperty("model", "um4_60");
        QVERIFY(!editor->property("valid").toBool());
        editor->setProperty("model", "gfs");
        QVERIFY(editor->property("valid").toBool());
        editor->setProperty("latitude", 91);
        QVERIFY(!editor->property("valid").toBool());
    }
    void desktopWindowStartsAtTheLocationRequestedByTheWidget()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/apps/meteo/Main.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const QVariantMap location{{"locationName", "Kielce"}, {"latitude", 50.8661}, {"longitude", 20.6286}, {"model", "um4_120"}};
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"smokeTest", true}, {"startupForecast", location}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *weather = object->findChild<WeatherModel *>();
        QVERIFY(weather);
        QCOMPARE(weather->locationName(), "Kielce");
        QCOMPARE(weather->latitude(), 50.8661);
        QCOMPARE(weather->longitude(), 20.6286);
        QCOMPARE(weather->model(), "um4_120");
        QVERIFY(!weather->autoRefresh());
        QVERIFY(object->property("startupReady").toBool());
    }
    void locationDialogUsesInsetContentAndFitsNarrowWindows()
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
        QCOMPARE(dialog->property("padding").toDouble(), 0);
        auto *content = object->findChild<QQuickItem *>("locationDialogContent");
        auto *search = object->findChild<QQuickItem *>("placeSearchField");
        auto *manual = object->findChild<QQuickItem *>("manualLocationToggle");
        QVERIFY(content); QVERIFY(search); QVERIFY(manual);
        QVERIFY(content->property("padding").toDouble() > 0);
        QVERIFY(dialog->property("height").toDouble() < window->height());
        const auto screenshotPath = qEnvironmentVariable("METEO_DIALOG_SCREENSHOT");
        if (!screenshotPath.isEmpty()) QVERIFY(window->grabWindow().save(screenshotPath));
        window->resize(420, 600);
        QTRY_VERIFY(dialog->property("width").toDouble() < window->width());
        QTRY_VERIFY(dialog->property("height").toDouble() < window->height());
        QTRY_VERIFY(search->width() > 100);
        QVERIFY(search->mapToItem(content, {0,0}).x() >= content->property("padding").toDouble());
        QVERIFY(search->mapToItem(content, {search->width(),0}).x() < content->width());
        QTest::qWait(100); // Allow the responsive form to finish layout after resizing.
        const auto collapsed = content->implicitHeight();
        manual->setProperty("checked", true);
        QTRY_VERIFY(content->implicitHeight() > collapsed + 90);
        manual->setProperty("checked", false);
        QTRY_VERIFY(qAbs(content->implicitHeight() - collapsed) < 2);
        QMetaObject::invokeMethod(dialog, "close");
    }
#ifdef METEO_TEST_PLASMA
    void nativeWidgetSectionsExpandAndCollapseIndependently()
    {
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|Binding loop|Cannot read|Cannot assign).*"));
        WeatherModel weather;
        weather.setAutoRefresh(false); weather.refresh(false);
        QVERIFY(weather.hasData());
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/ForecastPopup.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> popup(component.createWithInitialProperties({{"weather", QVariant::fromValue(&weather)},
            {"clockTime", weather.firstTimestamp() + 3600}}));
        QVERIFY2(popup, qPrintable(component.errorString()));
        QQuickWindow window; window.resize(540, 720);
        auto *item = qobject_cast<QQuickItem *>(popup.data());
        item->setParentItem(window.contentItem()); item->setSize(window.size()); window.show();
        auto *list = popup->findChild<QObject *>("widgetSections");
        QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(), 3);
        QQuickItem *details = nullptr, *temperature = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, details), Q_ARG(int, 0)) && details);
        QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, temperature), Q_ARG(int, 1)) && temperature);
        QVERIFY(!details->property("expanded").toBool());
        QVERIFY(temperature->property("expanded").toBool());
        QTRY_VERIFY(temperature->height() > 150);
        QVERIFY(QMetaObject::invokeMethod(temperature, "collapse"));
        QTRY_VERIFY(temperature->height() < 100);
        QVERIFY(QMetaObject::invokeMethod(details, "expand"));
        QTRY_VERIFY(details->height() > 100);
        QVERIFY(!temperature->property("expanded").toBool());
        QVERIFY(QMetaObject::invokeMethod(temperature, "expand"));
        QTRY_VERIFY(temperature->height() > 150);
    }
#endif
    void widgetPlaceSettingsValidateSelectionAndReorderPlaces()
    {
        QQmlEngine engine;
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        preferences->savePlace("Kielce", 50.8661, 20.6286);
        preferences->savePlace("Gdańsk", 54.352, 18.6466);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/configPlaces.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> settings(component.createWithInitialProperties({
            {"cfg_visiblePlaces", QStringList{"54.352000,18.646600", "50.866100,20.628600"}},
            {"cfg_placeOrder", QStringList{"54.352000,18.646600", "50.866100,20.628600"}}}));
        QVERIFY2(settings, qPrintable(component.errorString()));
        QVERIFY(settings->property("isSaveAllowed").toBool());
        QVERIFY(QMetaObject::invokeMethod(settings.data(), "move", Q_ARG(QVariant, 0), Q_ARG(QVariant, 1)));
        const auto order = qvariant_cast<QJSValue>(settings->property("cfg_placeOrder")).toVariant().toStringList();
        QCOMPARE(order[0], "50.866100,20.628600");
        QCOMPARE(order[1], "54.352000,18.646600");
        settings->setProperty("cfg_visiblePlaces", QStringList{});
        QVERIFY(!settings->property("isSaveAllowed").toBool());
        settings->setProperty("cfg_visiblePlaces", QStringList{"*"});
        QVERIFY(settings->property("isSaveAllowed").toBool());
    }
    void widgetChartsKeepTheirOwnProfileAndTimeRange()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop|Detected anchors).*")));
        WeatherModel model;
        model.setAutoRefresh(false);
        QQmlEngine engine;
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        QVERIFY(preferences);
        preferences->setProfile("classic");
        const auto id = preferences->createProfile("Widget profile");
        preferences->setChartOptions("temperature", {{"title", "Widget temperature"}, {"height", 130}});
        preferences->setProfileChartHeight(145);
        preferences->setProfile("basic");
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "WidgetForecast");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({
            {"weather", QVariant::fromValue(&model)}, {"profile", id}, {"hours", 24},
            {"clockTime", model.firstTimestamp() + 3600.0},
            {"chartKeys", QStringList{"temperature", "wind"}}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        model.refresh(false); // The popup is created before its forecast arrives.
        QVERIFY(model.hasData());
        const auto prepared = model.configuredCharts(preferences->chartLayoutForProfile(id));
        QVERIFY(prepared.size() >= 2);
        const auto widgetCharts = qvariant_cast<QJSValue>(object->property("charts")).toVariant().toList();
        QCOMPARE(widgetCharts.size(), 2);
        auto *repeater = object->findChild<QObject *>("widgetChartRepeater");
        QVERIFY(repeater);
        QCOMPARE(repeater->property("count").toInt(), 2);
        auto *root = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        window.resize(540, 660);
        root->setParentItem(window.contentItem()); root->setSize(window.size()); window.show();
        QQuickItem *temperature = nullptr, *wind = nullptr;
        QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Q_RETURN_ARG(QQuickItem *, temperature), Q_ARG(int, 0)));
        QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Q_RETURN_ARG(QQuickItem *, wind), Q_ARG(int, 1)));
        QVERIFY(temperature); QVERIFY(wind);
        QCOMPARE(temperature->objectName(), "chart-temperature");
        QCOMPARE(wind->objectName(), "chart-wind");
        QCOMPARE(temperature->window(), &window);
        QTRY_VERIFY(temperature->isVisible());
        QTRY_VERIFY(temperature->width() > 200);
        QTRY_VERIFY(temperature->height() > 100);
        QCOMPARE(temperature->property("title").toString(), "Widget temperature");
        QCOMPARE(temperature->property("chartHeight").toInt(), 130);
        QCOMPARE(wind->property("chartHeight").toInt(), 145);
        QCOMPARE(object->property("endTime").toLongLong() - object->property("startTime").toLongLong(), 24 * 3600);
        QCOMPARE(preferences->profile(), "basic");
        object->setProperty("hours", 48);
        QCOMPARE(object->property("endTime").toLongLong() - object->property("startTime").toLongLong(), std::min<qint64>(48 * 3600, model.lastTimestamp() - model.firstTimestamp()));
        object->setProperty("clockTime", model.lastTimestamp() + 86400.0);
        QCOMPARE(object->property("endTime").toLongLong(), model.lastTimestamp());
        object->setProperty("hours", 0);
        QCOMPARE(object->property("startTime").toLongLong(), model.firstTimestamp());
        QCOMPARE(object->property("endTime").toLongLong(), model.lastTimestamp());
        object->setProperty("chartKeys", QStringList{"wind"});
        QTRY_COMPARE(repeater->property("count").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Q_RETURN_ARG(QQuickItem *, wind), Q_ARG(int, 0)));
        QVERIFY(wind);
        QCOMPARE(wind->objectName(), "chart-wind");
        QCOMPARE(preferences->profile(), "basic");
    }
    void widgetConfigurationValidatesOneOrTwoCharts()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/configAppearance.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QVERIFY(object->property("isSaveAllowed").toBool());
        object->setProperty("cfg_chartKeys", QStringList{"wind"});
        QVERIFY(object->property("isSaveAllowed").toBool());
        object->setProperty("cfg_chartKeys", QStringList{});
        QVERIFY(!object->property("isSaveAllowed").toBool());
        object->setProperty("cfg_chartKeys", QStringList{"temperature", "precipitation", "wind"});
        QVERIFY(!object->property("isSaveAllowed").toBool());
        object->setProperty("cfg_popupView", "forecast");
        QVERIFY(object->property("isSaveAllowed").toBool());
    }
    void plasmaConfigurationDoesNotLayOutKirigamiSeparators()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*Detected anchors on an item.*")));
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import org.kde.kirigami as Kirigami
            Item {
                id: host
                property url configUrl
                property Item editor
                width: 1100; height: 800
                Kirigami.ColumnView {
                    id: columns
                    anchors.fill: parent
                    columnResizeMode: Kirigami.ColumnView.FixedColumns
                    columnWidth: 500
                    Component.onCompleted: {
                        host.editor = Qt.createComponent(host.configUrl).createObject(columns);
                        addItem(host.editor);
                    }
                }
            }
        )", QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const auto url = QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/configGeneral.qml"));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"configUrl", url}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *editor = qvariant_cast<QQuickItem *>(object->property("editor"));
        QVERIFY(editor);
        QQuickWindow window;
        window.resize(1100, 800);
        qobject_cast<QQuickItem *>(object.data())->setParentItem(window.contentItem());
        window.show();
        QTest::qWait(100);
        QVERIFY(editor->width() > 300);
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
