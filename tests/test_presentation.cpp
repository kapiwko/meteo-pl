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
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPointer>
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
#include <QSettings>

class PresentationTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_cache;
private slots:
    void firstRunRequiresAnExplicitPlace()
    {
        QFETCH(bool, mobile);
        QSettings settings;
        settings.beginGroup("Location");
        QVariantMap original;
        for (const auto &key : settings.childKeys()) original[key] = settings.value(key);
        settings.remove(""); settings.sync();
        const auto restore = qScopeGuard([&] {
            settings.remove("");
            for (auto it = original.cbegin(); it != original.cend(); ++it) settings.setValue(it.key(), it.value());
            settings.sync();
        });
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/apps/meteo/") + (mobile ? "Mobile.qml" : "Main.qml")));
        QScopedPointer<QObject> window(component.createWithInitialProperties({{"smokeTest", true}}));
        QVERIFY2(window, qPrintable(component.errorString()));
        auto *weather = window->findChild<WeatherModel *>("appForecastModel");
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        QVERIFY(weather && preferences);
        QTest::qWait(100);
        QVERIFY(weather->locationName().isEmpty());
        QVERIFY(preferences->favorites().isEmpty());
        QVERIFY(!weather->busy());
        QObject *setup = window.data();
        QObject *navigation = nullptr;
        if (mobile) {
            navigation = window->findChild<QObject *>("mobilePageStack");
            QVERIFY(navigation);
            setup = navigation->property("currentItem").value<QObject *>();
        }
        QVERIFY(setup);
        QVERIFY(QMetaObject::invokeMethod(setup, "beginSetup"));
        QObject *form = nullptr;
        if (mobile) {
            QTRY_COMPARE(navigation->property("depth").toInt(), 2);
            form = navigation->property("currentItem").value<QObject *>();
        } else form = window->findChild<QObject *>("locationDialog");
        QVERIFY(form);
        QVERIFY(form->property("firstRun").toBool());
        QCOMPARE(form->property("title").toString(), "Welcome to Meteo");
        auto *editor = form->findChild<QObject *>("locationEditor");
        QVERIFY(editor && !editor->property("valid").toBool());
        editor->setProperty("locationName", "A name without a point");
        QVERIFY(!editor->property("valid").toBool());
        if (mobile) {
            QTRY_VERIFY(!navigation->property("busy").toBool());
            QTest::keyClick(qobject_cast<QQuickWindow *>(window.data()), Qt::Key_Back);
            QTRY_COMPARE(navigation->property("depth").toInt(), 1);
            QTRY_VERIFY(!navigation->property("busy").toBool());
            QObject *panel = nullptr;
            for (auto *candidate : window->findChildren<QObject *>())
                if (QByteArray(candidate->metaObject()->className()).startsWith("ForecastPanel_")) { panel = candidate; break; }
            QVERIFY(panel);
            QVERIFY(QMetaObject::invokeMethod(panel, "addLocationRequested"));
            QTRY_COMPARE(navigation->property("depth").toInt(), 2);
            form = navigation->property("currentItem").value<QObject *>();
        } else {
            QVERIFY(QMetaObject::invokeMethod(form, "reject"));
            QVERIFY(QMetaObject::invokeMethod(setup, "beginSetup"));
        }
        QVERIFY(preferences->favorites().isEmpty());
        QVERIFY(weather->locationName().isEmpty());
        editor = form->findChild<QObject *>("locationEditor");
        QVERIFY(editor);
        if (mobile) QTRY_VERIFY(!navigation->property("busy").toBool());
        const QVariantMap place{{"name", "Kielce"}, {"latitude", 50.8661}, {"longitude", 20.6286}};
        QVERIFY(QMetaObject::invokeMethod(editor, "selectPlace", Q_ARG(QVariant, place)));
        if (mobile) {
            auto *save = form->findChild<QObject *>("mobileSavePlaceButton");
            QVERIFY(save && save->property("enabled").toBool());
            QVERIFY(QMetaObject::invokeMethod(save, "clicked"));
        } else QVERIFY(QMetaObject::invokeMethod(form, "save"));
        QTRY_COMPARE(weather->locationName(), "Kielce");
        if (mobile) QTRY_COMPARE(navigation->property("depth").toInt(), 1);
        QCOMPARE(preferences->favorites().size(), 1);
        QCOMPARE(preferences->favorites().first().toMap()["name"].toString(), "Kielce");
        QVERIFY(!preferences->isFavorite(52.2297, 21.0122));
    }
    void firstRunRequiresAnExplicitPlace_data()
    {
        QTest::addColumn<bool>("mobile");
        QTest::newRow("desktop") << false;
        QTest::newRow("mobile") << true;
    }
    void widgetLocationSettingsFollowAnEditedPlace()
    {
        QQmlEngine engine;
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        preferences->savePlace("Kielce", 50.8661, 20.6286);
        const auto id = preferences->placeAt(50.8661, 20.6286)["id"].toString();
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/configGeneral.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> settings(component.createWithInitialProperties({
            {"cfg_locationName", "Kielce"}, {"cfg_latitude", 50.8661}, {"cfg_longitude", 20.6286}, {"cfg_model", "um4_60"}}));
        QVERIFY2(settings, qPrintable(component.errorString()));
        QVERIFY(settings->property("isSaveAllowed").toBool());
        QVERIFY(preferences->editPlace(id, "Moved Kielce", 50.9, 20.7)["ok"].toBool());
        QTRY_COMPARE(settings->property("cfg_locationName").toString(), "Moved Kielce");
        QCOMPARE(settings->property("cfg_latitude").toDouble(), 50.9);
        QCOMPARE(settings->property("cfg_longitude").toDouble(), 20.7);
        QVERIFY(settings->property("isSaveAllowed").toBool());
        QVERIFY(preferences->editPlace(id, "Kielce", 50.8661, 20.6286)["ok"].toBool());
    }
    void placesManagerCanUndoConsecutiveRemovals()
    {
        QQmlEngine engine;
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        const auto first = preferences->editPlace({}, "First undo place", 55.1, 19.2)["place"].toMap();
        const auto second = preferences->editPlace({}, "Second undo place", 55.2, 19.3)["place"].toMap();
        QVERIFY(!first.isEmpty() && !second.isEmpty());
        const auto original = preferences->favorites();
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "PlacesView");
        QScopedPointer<QObject> view(component.create());
        QVERIFY2(view, qPrintable(component.errorString()));
        for (const auto &place : {first, second}) {
            QVERIFY(QMetaObject::invokeMethod(view.data(), "remove", Q_ARG(QVariant, place)));
            QCOMPARE(preferences->favorites().size(), original.size() - 1);
            QVERIFY(QMetaObject::invokeMethod(view.data(), "undoRemoval"));
            QCOMPARE(preferences->favorites(), original);
        }
        preferences->deletePlace(first["id"].toString());
        preferences->deletePlace(second["id"].toString());
    }
    void placeSaveButtonKeepsTheFormOpenOnErrorAndAcceptsOnce()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlEngine engine; QQmlComponent component(&engine);
        component.setData(R"(import QtQuick
            import pl.meteo.desktop
            Item { PlaceEditorDialog {} }
        )", QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window; window.resize(720, 900); item->setParentItem(window.contentItem()); item->setSize({720, 900}); window.show();
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        const auto saved = preferences->editPlace({}, "Existing", 50, 20)["place"].toMap();
        auto *dialog = object->findChild<QObject *>("locationDialog");
        auto *save = object->findChild<QQuickItem *>("saveLocationButton");
        auto *editor = object->findChild<QObject *>("locationEditor");
        QVERIFY(dialog && save && editor);
        dialog->setProperty("place", saved);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(preferences->deletePlace(saved["id"].toString())["ok"].toBool());
        QVERIFY(save->isEnabled());
        QSignalSpy accepted(dialog, SIGNAL(accepted()));
        const auto clickSave = [&] { QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, save->mapToScene({save->width()/2, save->height()/2}).toPoint()); };
        clickSave();
        QVERIFY(dialog->property("visible").toBool());
        QVERIFY(!dialog->property("saveError").toString().isEmpty());
        QCOMPARE(accepted.size(), 0);
        QCOMPARE(preferences->favorites().size(), 0);
        QVERIFY(QMetaObject::invokeMethod(dialog, "reject"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        dialog->setProperty("place", QVariantMap{});
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        const QVariantMap newPlace{{"name", "New place"}, {"latitude", 51}, {"longitude", 21}};
        QVERIFY(QMetaObject::invokeMethod(editor, "selectPlace", Q_ARG(QVariant, newPlace)));
        QVERIFY(save->isEnabled()); clickSave();
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(preferences->favorites().size(), 1);
    }
    void init()
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "meteo-pl", "desktop");
        settings.clear();
        settings.sync();
    }
    void initTestCase()
    {
        QVERIFY(m_cache.isValid());
        QCoreApplication::setOrganizationName("MeteoTest");
        QCoreApplication::setApplicationName("MeteoTest");
        QQuickStyle::setStyle(qEnvironmentVariable("METEO_TEST_STYLE", "org.kde.desktop"));
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
        QQuickWindow window; window.resize(720, 180);
        auto *item = qobject_cast<QQuickItem *>(header.data());
        item->setParentItem(window.contentItem()); item->setSize(window.size()); window.show();
        auto *symbol = header->findChild<QObject *>("headerWeatherSymbol");
        QVERIFY(symbol);
        auto *indicator = symbol->findChild<QObject *>("forecastLoadingIndicator");
        auto *icon = symbol->findChild<QObject *>("forecastWeatherIcon");
        auto *time = header->findChild<QObject *>("forecastSummaryTime");
        QVERIFY(indicator); QVERIFY(icon); QVERIFY(time);
        QTRY_VERIFY(!header->property("loading").toBool());
        QVERIFY(!indicator->property("running").toBool());
        QVERIFY(!icon->property("source").toString().isEmpty());
        QCOMPARE(icon->property("placeholder").toString(), QString());
        QTRY_COMPARE(icon->property("opacity").toDouble(), 1.0);
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
    void touchGesturesNavigateChartsWithoutBlockingVerticalScrolling()
    {
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|Binding loop|Cannot read|Cannot assign).*"));
        WeatherModel weather; weather.setAutoRefresh(false); weather.refresh(false);
        QVERIFY(weather.hasData());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "ForecastPanel");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> panel(component.createWithInitialProperties({{"weather", QVariant::fromValue(&weather)}, {"touchMode", true}}));
        QVERIFY2(panel, qPrintable(component.errorString()));
        QQuickWindow window; window.resize(600, 900);
        auto *item = qobject_cast<QQuickItem *>(panel.data());
        item->setParentItem(window.contentItem()); item->setSize(window.size()); window.show();
        const auto find = [](auto &&self, QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name) return parent;
            for (auto *child : parent->childItems()) if (auto *found = self(self, child, name)) return found;
            return nullptr;
        };
        QQuickItem *plot = nullptr, *scroll = nullptr;
        QTRY_VERIFY(plot = find(find, item, "plot-temperature"));
        QTRY_VERIFY(scroll = find(find, item, "meteogramScrollView"));
        auto *flickable = scroll->property("contentItem").value<QQuickItem *>();
        QVERIFY(flickable);
        auto *zoom = find(find, item, "chartZoomSlider");
        auto *time = find(find, item, "chartTimeSlider");
        QVERIFY(zoom && time && !zoom->isVisible() && !time->isVisible());
        QTest::qWait(100);
        const auto start = [&] { return panel->property("viewportStart").toDouble(); };
        const auto span = [&] { return panel->property("viewportEnd").toDouble() - start(); };
        const double fullSpan = span();
        static auto *device = QTest::createTouchDevice();
        auto touch = QTest::touchEvent(&window, device, false);
        const auto center = plot->mapToScene({plot->width() / 2, plot->height() / 2}).toPoint();
        touch.press(0, center - QPoint(60, 0)).press(1, center + QPoint(60, 0)).commit();
        for (int i = 1; i <= 6; ++i) {
            touch.move(0, center - QPoint(60 + 12*i, 0)).move(1, center + QPoint(60 + 12*i, 0)).commit();
            QTest::qWait(20);
        }
        touch.release(0, center - QPoint(132, 0)).release(1, center + QPoint(132, 0)).commit();
        QTRY_VERIFY(span() < fullSpan * 0.8);
        QVERIFY(span() >= 24 * 3600);
        QVERIFY(start() >= weather.firstTimestamp());
        QVERIFY(panel->property("viewportEnd").toDouble() <= weather.lastTimestamp());
        QVERIFY(qAbs(flickable->property("contentY").toDouble()) < 1);
        const double pinchedSpan = span(), pinchedStart = start();
        touch.press(0, center).commit();
        for (int i = 1; i <= 5; ++i) {
            touch.move(0, center + QPoint(12*i, 0)).commit();
            QTest::qWait(20);
        }
        touch.release(0, center + QPoint(60, 0)).commit();
        QTRY_VERIFY(start() < pinchedStart - 60);
        QVERIFY(qAbs(span() - pinchedSpan) < 1);
        const double draggedStart = start();
        touch.press(0, center).commit();
        for (int i = 1; i <= 5; ++i) {
            touch.move(0, center - QPoint(0, 16*i)).commit();
            QTest::qWait(20);
        }
        touch.release(0, center - QPoint(0, 80)).commit();
        QTRY_VERIFY(flickable->property("contentY").toDouble() > 20);
        QVERIFY(qAbs(start() - draggedStart) < 1);
        QVERIFY(qAbs(span() - pinchedSpan) < 1);
        QVERIFY(QMetaObject::invokeMethod(flickable, "cancelFlick"));
        flickable->setProperty("contentY", 0);
        QTest::qWait(100);
        touch.press(0, center).commit(); touch.release(0, center).commit();
        QTest::qWait(60);
        touch.press(0, center).commit(); touch.release(0, center).commit();
        QTRY_COMPARE(span(), fullSpan);
        QCOMPARE(start(), double(weather.firstTimestamp()));
        QTest::qWait(100); // Drain the final touch release before destroying its window.
    }
    void mobileNavigationAndSettingsUsePages()
    {
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|[Bb]inding loop|Cannot read|Cannot assign).*"));
        QQmlEngine engine;
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        // Material controls use the native Linux palette provider. Kirigami
        // 6.13's QML fallback dereferences destroyed pages during theme sync.
        engine.setProperty("_kirigamiTheme", QStringLiteral("org.kde.desktop"));
#endif
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/apps/meteo/Mobile.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const QVariantMap startupForecast{{"locationName", "Warszawa"}, {"latitude", 52.2297}, {"longitude", 21.0122}, {"model", "um4_60"}};
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"smokeTest", true}, {"startupForecast", startupForecast}, {"width", 360}, {"height", 800}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *weather = object->findChild<WeatherModel *>();
        QVERIFY(weather);
        QVERIFY(!weather->autoRefresh());
        weather->refresh(false);
        QVERIFY(weather->hasData());
        auto *window = qobject_cast<QQuickWindow *>(object.data());
        QVERIFY(window);
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        const bool materialStyle = QQuickStyle::name() == QStringLiteral("Material");
        if (materialStyle && qEnvironmentVariable("QT_QUICK_CONTROLS_MATERIAL_THEME") != QStringLiteral("Dark")) {
            QVERIFY(object->setProperty("colorScheme", QVariant::fromValue(Qt::ColorScheme::Dark)));
            QTRY_VERIFY(window->color().lightnessF() < 0.5);
            QVERIFY(object->setProperty("colorScheme", QVariant::fromValue(Qt::ColorScheme::Light)));
            QTRY_VERIFY(window->color().lightnessF() > 0.5);
        }
        const auto find = [](auto &&self, QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name) return parent;
            for (auto *child : parent->childItems()) if (auto *found = self(self, child, name)) return found;
            return nullptr;
        };
        const auto findButton = [](auto &&self, QQuickItem *parent, const QString &text) -> QQuickItem * {
            if (parent->isVisible() && parent->property("text").toString() == text && parent->property("checkable").isValid()) return parent;
            for (auto *child : parent->childItems()) if (auto *found = self(self, child, text)) return found;
            return nullptr;
        };
        const auto click = [window](QQuickItem *button) {
            auto *stack = window->findChild<QObject *>("mobilePageStack");
            if (stack) QTRY_VERIFY(!stack->property("busy").toBool());
            QTRY_VERIFY(button->isVisible() && button->width() > 0
                && button->mapToScene({button->width()/2, button->height()/2}).x() >= 0
                && button->mapToScene({button->width()/2, button->height()/2}).x() < window->width()
                && button->mapToScene({button->width()/2, button->height()/2}).y() >= 0
                && button->mapToScene({button->width()/2, button->height()/2}).y() < window->height());
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, button->mapToScene({button->width()/2, button->height()/2}).toPoint());
        };
        QQuickItem *page = nullptr, *navigation = nullptr;
        QTRY_VERIFY(page = find(find, window->contentItem(), "mobileForecastPage"));
        QTRY_VERIFY(navigation = find(find, page, "mobileNavigation"));
        QTRY_COMPARE(navigation->property("count").toInt(), 3);
        auto *stack = page->property("navigationStack").value<QObject *>();
        QVERIFY(stack);
        auto *panel = page->property("panel").value<QObject *>();
        QVERIFY(panel);
        QQuickItem *temperature = nullptr, *temperaturePlot = nullptr;
        QTRY_VERIFY(temperature = find(find, page, "chart-temperature"));
        QTRY_VERIFY(temperaturePlot = find(find, page, "plot-temperature"));
        QTRY_VERIFY(temperaturePlot->width() > 200);
        const double plotWidth = temperaturePlot->width() - temperature->property("leftMargin").toDouble() - temperature->property("rightMargin").toDouble();
        QVERIFY2(plotWidth >= window->width() * 0.75, "Mobile charts waste too much screen width outside the data area");
        auto *meteogram = findButton(findButton, page, "Meteogram");
        QVERIFY(meteogram);
        QCOMPARE(QQmlProperty(meteogram, "icon.name").read().toString(), "office-chart-line");
        QQuickItem *placeButton = nullptr;
        QTRY_VERIFY(placeButton = find(find, page, "mobilePlaceButton"));
        click(placeButton);
        auto *places = page->findChild<QObject *>("mobilePlacesSheet");
        QVERIFY(places);
        QTRY_VERIFY(places->property("opened").toBool());
        auto *sheetBody = places->property("contentItem").value<QQuickItem *>();
        QVERIFY(sheetBody);
        auto *addPlace = findButton(findButton, sheetBody, "Add location");
        QVERIFY(addPlace);
        QVERIFY(places->property("height").toDouble() >= addPlace->height());
        QCOMPARE(places->property("width").toDouble(), double(window->width()));
        click(addPlace);
        QTRY_COMPARE(stack->property("depth").toInt(), 2);
        auto *addPage = stack->property("currentItem").value<QQuickItem *>();
        QVERIFY(addPage);
        QTRY_VERIFY(qAbs(addPage->mapToScene({0, 0}).x()) < 1);
        QTRY_VERIFY(!stack->property("busy").toBool());
        QCOMPARE(addPage->objectName(), "mobileLocationPage");
        QVERIFY(!addPage->property("editing").toBool());
        auto *newPlaceButton = addPage->findChild<QObject *>("mobileSavePlaceButton");
        QVERIFY(newPlaceButton);
        QVERIFY(!newPlaceButton->property("enabled").toBool());
        QCOMPARE(addPage->findChild<QObject *>("customLocationName")->property("text").toString(), QString());
        QTRY_VERIFY(!stack->property("busy").toBool());
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_COMPARE(stack->property("depth").toInt(), 1);
        QTRY_VERIFY(!stack->property("busy").toBool());
        QTRY_VERIFY(!places->property("visible").toBool());
        auto *moreButton = find(find, page, "mobileMoreButton");
        QVERIFY(moreButton); click(moreButton);
        auto *more = page->findChild<QObject *>("mobileMoreSheet");
        QVERIFY(more);
        QTRY_VERIFY(more->property("opened").toBool());
        QVERIFY(more->property("leftPadding").toDouble() > 0);
        QVERIFY(more->property("topPadding").toDouble() > 0);
        QVERIFY(more->property("height").toDouble() > more->property("topPadding").toDouble() + more->property("bottomPadding").toDouble());
        QVERIFY(more->property("bottomPadding").toDouble() >= more->property("topPadding").toDouble());
        window->resize(360, 700);
        QTRY_VERIFY(more->property("height").toDouble() <= window->height() * 0.8);
        window->resize(360, 800);
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_VERIFY(!more->property("visible").toBool());
        QTRY_VERIFY(navigation->mapToScene({0, 0}).y() > window->height()/2);
        auto *desktopNavigation = find(find, page, "forecastNavigation");
        QVERIFY(desktopNavigation && !desktopNavigation->isVisible());
        auto *zoom = find(find, page, "chartZoomSlider");
        QVERIFY(zoom && !zoom->isVisible());
        QVERIFY(QMetaObject::invokeMethod(panel, "setHours", Q_ARG(QVariant, 24)));
        auto *hourly = findButton(findButton, page, "Hourly");
        QVERIFY(hourly); click(hourly);
        QCOMPARE(QQmlProperty(hourly, "icon.name").read().toString(), "chronometer");
        QTRY_COMPARE(panel->property("viewMode").toInt(), 1);
        QQuickItem *hours = nullptr, *firstHour = nullptr;
        QTRY_VERIFY(hours = find(find, page, "mobileHourlyForecast"));
        QTRY_VERIFY(firstHour = find(find, hours, "mobileForecastHour-0"));
        QTRY_VERIFY(hours->width() > 100);
        QCOMPARE(hours->property("count").toInt(), weather->hourlyForecast().size());
        QVERIFY(firstHour->mapToScene({firstHour->width(), 0}).x() <= window->width() + 1);
        QVERIFY(hours->property("contentHeight").toDouble() > hours->height());
        auto *days = findButton(findButton, page, "Days");
        QVERIFY(days); click(days);
        QCOMPARE(QQmlProperty(days, "icon.name").read().toString(), "view-calendar");
        QTRY_COMPARE(panel->property("viewMode").toInt(), 2);
        auto *news = findButton(findButton, navigation, "News");
        QVERIFY(news); click(news);
        QTRY_COMPARE(page->property("destination").toInt(), 2);
        QVERIFY(!placeButton->isVisible());
        auto *synoptic = findButton(findButton, page, "Synoptic");
        QVERIFY(synoptic);
        QCOMPARE(QQmlProperty(synoptic, "icon.name").read().toString(), "dialog-messages");
        auto *articles = findButton(findButton, page, "Articles");
        QVERIFY(articles); click(articles);
        QCOMPARE(QQmlProperty(articles, "icon.name").read().toString(), "view-list-text");
        QTRY_COMPARE(panel->property("viewMode").toInt(), 5);
        auto *articleReader = find(find, page, "editorialView");
        QVERIFY(articleReader);
        articleReader->setProperty("selectedArticle", QVariantMap{{"title", "Article"}, {"html", "<p>Fixture article</p>"}, {"url", "https://www.meteo.pl/"}});
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_COMPARE(panel->property("viewMode").toInt(), 5);
        QTRY_VERIFY(articleReader->property("selectedArticle").isNull());
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_COMPARE(page->property("destination").toInt(), 0);
        QVERIFY(placeButton->isVisible());
        auto *forecast = findButton(findButton, navigation, "Forecast");
        QVERIFY(forecast); click(forecast);
        QTRY_COMPARE(panel->property("viewMode").toInt(), 2);
        QCOMPARE(panel->property("viewportEnd").toDouble() - panel->property("viewportStart").toDouble(), 24*3600.0);
        auto *maps = findButton(findButton, navigation, "Maps");
        QVERIFY(maps); click(maps);
        QTRY_COMPARE(panel->property("viewMode").toInt(), 3);
        QVERIFY(!placeButton->isVisible());
        QQuickItem *mapType = nullptr, *mapViewport = nullptr;
        QTRY_VERIFY(mapType = find(find, page, "mobileMapType"));
        QTRY_VERIFY(mapViewport = find(find, page, "weatherMapViewport"));
        QTRY_VERIFY(mapViewport->height() >= 160);
        QVERIFY(mapType->mapToScene({mapType->width(), 0}).x() <= window->width() + 1);
        QVERIFY(mapViewport->mapToScene({0, mapViewport->height()}).y() <= navigation->mapToScene({0, 0}).y() + 1);
        auto *mapZoomIn = find(find, mapViewport, "mapZoomIn");
        auto *mapZoomOut = find(find, mapViewport, "mapZoomOut");
        QVERIFY(mapZoomIn && mapZoomOut);
        QVERIFY(mapZoomIn->width() >= 48 && mapZoomIn->height() >= 48);
        const auto originalZoom = mapViewport->property("zoom").toInt();
        click(mapZoomIn);
        QTRY_COMPARE(mapViewport->property("zoom").toInt(), originalZoom + 1);
        click(mapZoomOut);
        QTRY_COMPARE(mapViewport->property("zoom").toInt(), originalZoom);
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_COMPARE(panel->property("viewMode").toInt(), 2);
        QVERIFY(placeButton->isVisible());
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_COMPARE(panel->property("viewMode").toInt(), 0);
        click(forecast);
        QTRY_COMPARE(panel->property("viewMode").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(page, "openSettings"));
        QTRY_COMPARE(stack->property("depth").toInt(), 2);
        auto currentPage = [stack] { return stack->property("currentItem").value<QQuickItem *>(); };
        QTRY_VERIFY(currentPage() && currentPage()->objectName() == "mobileSettingsPage");
        QTRY_VERIFY(qAbs(currentPage()->mapToScene({0, 0}).x()) < 1);
        QTRY_VERIFY(!stack->property("busy").toBool());
        QTest::keySequence(window, QKeySequence(QKeySequence::Back));
        QTRY_COMPARE(stack->property("depth").toInt(), 1);
        QTRY_VERIFY(!stack->property("busy").toBool());
        QTRY_VERIFY(qAbs(page->mapToScene({0, 0}).x()) < 1);
        QVERIFY(QMetaObject::invokeMethod(page, "openSettings"));
        QTRY_COMPARE(stack->property("depth").toInt(), 2);
        QTRY_VERIFY(currentPage() && currentPage()->objectName() == "mobileSettingsPage");
        QTRY_VERIFY(qAbs(currentPage()->mapToScene({0, 0}).x()) < 1);
        QQuickItem *application = nullptr;
        QTRY_VERIFY(application = find(find, currentPage(), "mobileSettingsSection-3"));
        click(application);
        QTRY_COMPARE(stack->property("depth").toInt(), 3);
        QTRY_VERIFY(currentPage() && currentPage()->objectName() == "mobileSettingsDetail-3");
        QTRY_VERIFY(qAbs(currentPage()->mapToScene({0, 0}).x()) < 1);
        QQuickItem *renderer = nullptr;
        QTRY_VERIFY(renderer = find(find, currentPage(), "rendererSetting"));
        QTRY_VERIFY(renderer->width() > 100);
        QVERIFY(renderer->mapToScene({renderer->width(),0}).x() <= window->width() + 1);
        QQuickItem *back = nullptr;
        QTRY_VERIFY(back = find(find, currentPage(), "mobileBackButton"));
        click(back);
        QTRY_COMPARE(stack->property("depth").toInt(), 2);
        QTRY_VERIFY(currentPage() && currentPage()->objectName() == "mobileSettingsPage");
        QTRY_VERIFY(qAbs(currentPage()->mapToScene({0, 0}).x()) < 1);
        QQuickItem *profilesEntry = nullptr;
        QTRY_VERIFY(profilesEntry = find(find, currentPage(), "mobileSettingsSection-1"));
        click(profilesEntry);
        QTRY_COMPARE(stack->property("depth").toInt(), 3);
        QTRY_VERIFY(currentPage() && currentPage()->objectName() == "mobileSettingsDetail-1");
        QTRY_VERIFY(!stack->property("busy").toBool());
        QQuickItem *profileEditor = nullptr;
        QTRY_VERIFY(profileEditor = find(find, currentPage(), "meteogramDisplaySettings"));
        QQuickItem *profileActions = nullptr;
        QTRY_VERIFY(profileActions = find(find, currentPage(), "profileActionsButton"));
        click(profileActions);
        QPointer<QObject> profileMenu = object->findChild<QObject *>("profileActionsMenu");
        QVERIFY(profileMenu);
        QTRY_VERIFY(profileMenu->property("opened").toBool());
        const auto depthBeforeMenuBack = stack->property("depth").toInt();
        QTest::keyClick(window, Qt::Key_Back);
        const auto depthAfterMenuBack = stack->property("depth").toInt();
        QCOMPARE(depthAfterMenuBack, depthBeforeMenuBack);
        QTRY_VERIFY(profileMenu && !profileMenu->property("visible").toBool());
        profileEditor->setProperty("expandedChart", "temperature");
        profileEditor->setProperty("expandedSeries", "temperature/airtmp_point");
        QObject *seriesDialog = nullptr;
        QTRY_VERIFY(seriesDialog = object->findChild<QObject *>("seriesAppearanceDialog"));
        QTRY_VERIFY(seriesDialog->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_COMPARE(profileEditor->property("expandedSeries").toString(), QString());
        QCOMPARE(stack->property("depth").toInt(), 3);
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_COMPARE(stack->property("depth").toInt(), 2);
        QTRY_VERIFY(!stack->property("busy").toBool());
        QTRY_VERIFY(currentPage() && currentPage()->objectName() == "mobileSettingsPage");
        QTRY_VERIFY(back = find(find, currentPage(), "mobileBackButton"));
        click(back);
        QTRY_COMPARE(stack->property("depth").toInt(), 1);
        QTRY_VERIFY(!stack->property("busy").toBool());
        QVERIFY(QMetaObject::invokeMethod(page, "chooseLocation"));
        QTRY_COMPARE(stack->property("depth").toInt(), 2);
        QTRY_VERIFY(currentPage() && currentPage()->objectName() == "mobilePlacesPage");
        QTRY_VERIFY(qAbs(currentPage()->mapToScene({0, 0}).x()) < 1);
        QQuickItem *manageAddPlace = nullptr;
        QTRY_VERIFY(manageAddPlace = find(find, currentPage(), "addPlaceButton"));
        click(manageAddPlace);
        QTRY_COMPARE(stack->property("depth").toInt(), 3);
        QTRY_VERIFY(currentPage() && currentPage()->objectName() == "mobileLocationPage");
        QTRY_VERIFY(!stack->property("busy").toBool());
        QQuickItem *savePlace = nullptr;
        QTRY_VERIFY(savePlace = find(find, currentPage(), "mobileSavePlaceButton"));
        QVERIFY(!savePlace->isEnabled());
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_COMPARE(stack->property("depth").toInt(), 2);
        QTRY_VERIFY(!stack->property("busy").toBool());
        QTest::keyClick(window, Qt::Key_Back);
        QTRY_COMPARE(stack->property("depth").toInt(), 1);
        QTRY_VERIFY(!stack->property("busy").toBool());
        QVERIFY(!window->grabWindow().isNull());
        if (const auto path = qEnvironmentVariable("METEO_MOBILE_SCREENSHOT"); !path.isEmpty()) QVERIFY(window->grabWindow().save(path));
    }
    void mobileSynopticSwipesChangeDatesAndKeepVerticalScrolling()
    {
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|[Bb]inding loop|Cannot read|Cannot assign).*"));
        const auto today = QDate::currentDate();
        const auto folder = m_cache.path() + "/meteo-pl/editorial";
        QVERIFY(QDir().mkpath(folder));
        for (const auto &suffix : QStringList{"", today.toString(Qt::ISODate) + '/', today.addDays(-1).toString(Qt::ISODate) + '/'}) {
            const auto url = "https://meteo-backend.dev.meteo.pl/api/v1/synoptic-comment/" + suffix + "pl";
            const auto hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha256).toHex();
            const auto date = suffix.isEmpty() ? today.toString(Qt::ISODate) : suffix.first(10);
            const auto html = QString("<p>A long synoptic commentary for testing horizontal and vertical touch navigation.</p>").repeated(100);
            QFile file(folder + '/' + hash + ".json");
            QVERIFY(file.open(QIODevice::WriteOnly));
            const QJsonObject envelope{{"url", url}, {"savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                {"document", QJsonObject{{"content", html}, {"published_date", date}}}};
            file.write(QJsonDocument(envelope).toJson());
        }
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "EditorialView");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> view(component.createWithInitialProperties({{"mobileLayout", true}}));
        QVERIFY2(view, qPrintable(component.errorString()));
        QQuickWindow window; window.resize(360, 640);
        auto *item = qobject_cast<QQuickItem *>(view.data());
        item->setParentItem(window.contentItem()); item->setSize(window.size()); window.show();
        auto *reader = view->findChild<QQuickItem *>("editorialReader");
        QVERIFY(reader);
        QTRY_VERIFY(reader->height() > 200);
        QTRY_COMPARE(view->property("shownDate").toString(), today.toString(Qt::ISODate));
        QVERIFY(!view->findChild<QQuickItem *>("synopticPreviousDay")->isVisible());
        QVERIFY(!view->findChild<QQuickItem *>("synopticNextDay")->isVisible());
        static auto *device = QTest::createTouchDevice();
        const auto swipe = [&](QPoint delta) {
            const auto origin = reader->mapToScene({reader->width()/2, reader->height()/2}).toPoint();
            auto touch = QTest::touchEvent(&window, device, false);
            touch.press(0, origin).commit();
            for (int step = 1; step <= 6; ++step) {
                touch.move(0, origin + delta * step / 6).commit();
                QTest::qWait(10);
            }
            touch.release(0, origin + delta).commit();
        };
        swipe({-130, 0}); // Future dates stay unavailable.
        QCOMPARE(view->property("shownDate").toString(), today.toString(Qt::ISODate));
        swipe({130, 0});
        QTRY_COMPARE(view->property("shownDate").toString(), today.addDays(-1).toString(Qt::ISODate));
        swipe({-130, 0});
        QTRY_COMPARE(view->property("shownDate").toString(), today.toString(Qt::ISODate));
        auto *flickable = reader->property("contentItem").value<QQuickItem *>();
        QVERIFY(flickable);
        swipe({0, -130});
        QTRY_VERIFY(flickable->property("contentY").toDouble() > 20);
        QCOMPARE(view->property("shownDate").toString(), today.toString(Qt::ISODate));
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
        preferences.setShowLegend(true); // This case compares exports with and without a visible legend.
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
        // The category change rebuilds the grid. Export only after the scene
        // has processed its layout, while keeping exporter revision checks on.
        QSignalSpy frames(&window, &QQuickWindow::afterAnimating);
        window.requestUpdate();
        QTRY_VERIFY(frames.count() >= 2);
        QCOMPARE(readings->height(), readings->implicitHeight());
        const auto pngPath = QDir(directory).filePath("meteo-reading-all.png");
        QVERIFY(QMetaObject::invokeMethod(object.data(), "exportReadingsToFile", Q_RETURN_ARG(QVariant, accepted),
            Q_ARG(QVariant, QUrl::fromLocalFile(pngPath)), Q_ARG(QVariant, QStringLiteral("png"))));
        QVERIFY(accepted.toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!saved.isEmpty() || !failed.isEmpty(), 18000);
        QVERIFY2(failed.isEmpty(), qPrintable(exporter->error()));
        QCOMPARE(saved.count(), 1);
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
        const auto conflict = model.configuredCharts({QVariantMap{{"key", "chart:mixed"}, {"enabled", true},
            {"series", QVariantList{defaultSeriesStyle("airtmp_point"), defaultSeriesStyle("wind10_sd_true_prev_point"), defaultSeriesStyle("slpres_point")}}}}).first().toMap();
        chart->setProperty("fields", conflict["fields"]);
        chart->setProperty("chartConfiguration", conflict);
        auto *notice = chart->findChild<QQuickItem *>("chartAxisConflict");
        auto *plot = chart->findChild<QQuickItem *>("plot-chart:mixed");
        QVERIFY(notice); QVERIFY(plot);
        QTRY_VERIFY(notice->isVisible());
        QVERIFY(!plot->isVisible());
        ChartExporter exporter;
        QTemporaryDir files;
        const auto png = files.filePath("conflicting-axes.png");
        QVERIFY(!exporter.savePng(QUrl::fromLocalFile(png), {QVariant::fromValue(chart.data())}, {}));
        QVERIFY(exporter.error().contains("axis conflict"));
        QVERIFY(!QFile::exists(png));
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
            QObject *appearance = nullptr;
            QTRY_VERIFY(appearance = object->findChild<QObject *>("seriesAppearanceDialog"));
            QTRY_VERIFY(appearance->property("opened").toBool());
            QTRY_VERIFY(qAbs(chart->height() - openHeight) < 2);
            QVERIFY(QMetaObject::invokeMethod(appearance, "close"));
            QTRY_COMPARE(object->property("expandedSeries").toString(), QString());
            QTRY_VERIFY(qAbs(chart->height() - openHeight) < 2);
            object->setProperty("expandedChart", "");
            QTRY_VERIFY(qAbs(chart->height() - closedHeight) < 2);
            QTRY_VERIFY(qAbs(item->implicitHeight() - closedTotal) < 2);
        }
    }
    void seriesAppearanceStaysOpenWhenItsProfileChanges()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "DisplaySettings");
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        window.resize(720, 900);
        item->setParentItem(window.contentItem());
        item->setWidth(680);
        window.show();
        object->setProperty("expandedChart", "temperature");
        object->setProperty("expandedSeries", "temperature/airtmp_point");
        QPointer<QObject> appearance;
        QTRY_VERIFY(appearance = object->findChild<QObject *>("seriesAppearanceDialog"));
        QTRY_VERIFY(appearance->property("opened").toBool());
        auto *original = appearance.data();
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        preferences->setSeriesStyle("temperature", "airtmp_point", {{"lineWidth", 3.5}, {"color", "#123456"}});
        QTest::qWait(150);
        QVERIFY(appearance);
        QCOMPARE(object->findChild<QObject *>("seriesAppearanceDialog"), original);
        QVERIFY(appearance->property("opened").toBool());
        const auto value = appearance->property("seriesStyle");
        const auto style = value.metaType() == QMetaType::fromType<QJSValue>()
            ? value.value<QJSValue>().toVariant().toMap() : value.toMap();
        QCOMPARE(style.value("lineWidth").toDouble(), 3.5);
        QCOMPARE(style.value("color").toString(), "#123456");
        QTest::keyClick(&window, Qt::Key_Back);
        QTRY_COMPARE(object->property("expandedSeries").toString(), QString());
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
        const auto id = preferences->favorites().first().toMap()["id"].toString();
        QVERIFY(preferences->editPlace(id, "Port", 54.4, 18.7)["ok"].toBool());
        QTRY_VERIFY(!changed.isEmpty() && !otherChanged.isEmpty()); apply();
        QTRY_COMPARE(model.locationName(), "Port");
        QCOMPARE(model.latitude(), 54.4);
        QCOMPARE(other.latitude(), 54.4);
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
        QVERIFY(duplicate->isEnabled()); QVERIFY(remove->isEnabled());
        const auto click = [&](QQuickItem *control) {
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
                control->mapToScene({control->width()/2, control->height()/2}).toPoint());
        };
        auto *actions = object->findChild<QQuickItem *>("profileActionsButton");
        auto *menu = object->findChild<QObject *>("profileActionsMenu");
        QVERIFY(actions && menu);
        const auto trigger = [&](QQuickItem *control) {
            click(actions);
            if (!QTest::qWaitFor([&] { return menu->property("opened").toBool(); })) return false;
            click(control);
            return QTest::qWaitFor([&] { return !menu->property("visible").toBool(); });
        };
        const auto count = preferences->profiles().size();
        QVERIFY(trigger(duplicate));
        QTRY_VERIFY(object->property("profileId").toString().startsWith("user:"));
        QCOMPARE(preferences->profile(), "classic");
        QCOMPARE(preferences->profiles().size(), count + 1);
        QVERIFY(remove->isEnabled()); QVERIFY(!name->property("readOnly").toBool());
        name->setProperty("text", "Profil testowy");
        QVERIFY(QMetaObject::invokeMethod(name, "editingFinished"));
        QCOMPARE(preferences->profiles().last().toMap()["name"].toString(), "Profil testowy");
        QVERIFY(trigger(remove));
        QTRY_COMPARE(object->property("profileId").toString(), QString("classic"));
        QCOMPARE(preferences->profile(), "classic");
        QCOMPARE(preferences->profiles().size(), count);
        auto *reset = object->findChild<QQuickItem *>("profileRestoreButton");
        auto *recover = object->findChild<QQuickItem *>("restoreDeletedProfilesButton");
        auto *addChart = object->findChild<QQuickItem *>("addChartButton");
        QVERIFY(reset && recover && addChart);
        const auto factory = preferences->chartLayout();
        click(addChart);
        auto *ground = object->findChild<QObject *>("addChart-ground");
        QVERIFY(ground);
        QVERIFY(QMetaObject::invokeMethod(ground, "triggered"));
        auto *chartMenu = object->findChild<QObject *>("addChartMenu");
        QVERIFY(chartMenu);
        QTRY_VERIFY(!chartMenu->property("visible").toBool());
        QTRY_COMPARE(preferences->chartLayout().size(), factory.size() + 1);
        QCOMPARE(preferences->profile(), "classic");
        QTRY_VERIFY(reset->isEnabled());
        QVERIFY(trigger(reset));
        QTRY_COMPARE(preferences->chartLayout(), factory);
        QTRY_VERIFY(!reset->isEnabled());
        QVERIFY(trigger(remove));
        QTRY_COMPARE(preferences->profiles().size(), count - 1);
        click(actions);
        QTRY_VERIFY(menu->property("opened").toBool());
        QTRY_VERIFY(recover->isVisible());
        click(recover);
        QTRY_COMPARE(preferences->profiles().size(), count);
        QVERIFY(!preferences->hasDeletedProfiles());
    }
    void responsiveSettingsAndForecastControls_data()
    {
        QTest::addColumn<int>("width");
        QTest::newRow("narrow") << 420;
        QTest::newRow("medium") << 720;
        QTest::newRow("wide") << 1280;
    }
    void responsiveSettingsAndForecastControls()
    {
        QFETCH(int, width);
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/apps/meteo/Main.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"smokeTest", true}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(object.data());
        QVERIFY(window);
        window->resize(width, 1000);
        auto *weather = object->findChild<WeatherModel *>();
        QVERIFY(weather);
        weather->refresh(false);
        QVERIFY(weather->hasData());
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        preferences->setProfile("basic");
        auto *exportButton = object->findChild<QQuickItem *>("chartExportButton");
        QVERIFY(exportButton);
        QTRY_VERIFY(exportButton->height() >= 20);
        const auto fits = [](QQuickItem *item, QQuickItem *container) {
            const auto x = item->mapToItem(container, {0, 0}).x();
            return item->width() > 0 && x >= -1 && x + item->width() <= container->width() + 1;
        };
        const auto items = [&](const QString &name) {
            QList<QQuickItem *> result;
            const auto visit = [&](const auto &self, QQuickItem *parent) -> void {
                for (auto *child : parent->childItems()) {
                    if (child->objectName() == name) result.append(child);
                    self(self, child);
                }
            };
            visit(visit, window->contentItem());
            return result;
        };
        QTRY_VERIFY(fits(exportButton, window->contentItem()));
        QObject *panel = nullptr;
        for (auto *candidate : object->findChildren<QObject *>())
            if (QByteArray(candidate->metaObject()->className()).startsWith("ForecastPanel_")) { panel = candidate; break; }
        QVERIFY(panel);
        panel->setProperty("viewMode", 2);
        QTRY_VERIFY(!items("forecastDayCard").isEmpty());
        for (auto *card : items("forecastDayCard")) {
            QTRY_VERIFY(fits(card, window->contentItem()));
        }
        panel->setProperty("viewMode", 0);
        QVERIFY(QMetaObject::invokeMethod(panel, "openSettings"));
        auto *dialog = object->findChild<QObject *>("meteoSettingsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *content = qvariant_cast<QQuickItem *>(dialog->property("contentItem"));
        QVERIFY(content);
        auto *display = object->findChild<QObject *>("meteogramDisplaySettings");
        QVERIFY(display);
        display->setProperty("expandedChart", "temperature");
        for (int section = 0; section < 4; ++section) {
            dialog->setProperty("section", section);
            if (section == 1) display->setProperty("expandedSeries", "temperature/airtmp_point");
            QTest::qWait(120);
            for (const auto &name : {"profileNameField", "profileActionsButton",
                     "seriesColor-temperature-airtmp_point-color", "seriesColor-temperature-airtmp_point-darkColor",
                     "chartBound-temperature-leftMin", "chartBound-temperature-leftMax",
                     "chartBound-temperature-rightMin", "chartBound-temperature-rightMax",
                     "settingsTimeRange", "probeModeSetting", "rendererSetting", "settingsFilePath"}) {
                if (QByteArray(name).startsWith("seriesColor-") && section != 1) continue;
                auto *control = items(QString::fromLatin1(name)).value(0);
                QVERIFY2(control, name);
                if (control->isVisible()) QTRY_VERIFY2(fits(control, content), name);
            }
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
            for (int i = 0; i < 4; ++i) result.append(QVariantMap{{"time", first + i * 3600}, {"value", value}, {"interval", 3600}});
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
        QTRY_VERIFY(plot->height() > 40 && plot->width() > 0);
        QVariant boundary;
        QVERIFY(QMetaObject::invokeMethod(chart.data(), "px", Q_RETURN_ARG(QVariant, boundary), Q_ARG(QVariant, first + 3600)));
        const auto nearBaseline = [&] {
            QMetaObject::invokeMethod(chart.data(), "px", Q_RETURN_ARG(QVariant, boundary), Q_ARG(QVariant, first + 3600));
            return plot->mapToScene({boundary.toDouble(), plot->height() - 40}).toPoint();
        };
        QTRY_COMPARE(window.grabWindow().pixelColor(nearBaseline()), QColor("#00aa00"));
        const auto pixel = nearBaseline();
        const auto image = window.grabWindow();
        for (int offset = -2; offset <= 2; ++offset) QCOMPARE(image.pixelColor(pixel + QPoint(offset, 0)), QColor("#00aa00"));
        const QVariantMap fog{{"key", "fog_max"}, {"label", "Fog"}, {"unit", "%"}, {"color", "#ffb400"}, {"kind", "rightBars"}, {"points", points(100)}};
        const QVariantMap clouds{{"key", "cldlow_aver"}, {"label", "Clouds"}, {"unit", "%"}, {"color", "#5a5a5a"}, {"kind", "cloudArea"}, {"points", points(100)}};
        chart->setProperty("groupKey", "clouds");
        chart->setProperty("fields", QVariantList{fog, clouds}); // deliberately reversed layer order
        QVERIFY(QMetaObject::invokeMethod(chart.data(), "px", Q_RETURN_ARG(QVariant, boundary), Q_ARG(QVariant, first + 3600)));
        QTRY_COMPARE(window.grabWindow().pixelColor(nearBaseline()), QColor("#ffb400"));

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
    void nativeSamplingIsSharedByReadingsCursorAndCsv_data()
    {
        QTest::addColumn<QTime>("startTime");
        QTest::addColumn<QString>("labelFormat");
        QTest::newRow("same-day") << QTime(12, 0) << QStringLiteral("HH:mm");
        QTest::newRow("across-midnight") << QTime(23, 0) << QStringLiteral("d MMM HH:mm");
    }
    void nativeSamplingIsSharedByReadingsCursorAndCsv()
    {
        QFETCH(QTime, startTime);
        QFETCH(QString, labelFormat);
        const auto hash = QCryptographicHash::hash("um4_60:52.229700:21.012200", QCryptographicHash::Sha256).toHex();
        const auto path = m_cache.path() + "/meteo-pl/" + hash + ".json";
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll(); file.close();
        const auto restore = qScopeGuard([&] { QFile output(path); if (output.open(QIODevice::WriteOnly)) output.write(original); });
        auto envelope = QJsonDocument::fromJson(original).object();
        auto payload = QJsonDocument::fromJson(envelope["payload"].toString().toUtf8()).object();
        auto fields = payload["data"].toObject();
        const auto first = QDateTime(QDate(2026, 10, 27), startTime, QTimeZone::systemTimeZone()).toSecsSinceEpoch();
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            auto field = it.value().toObject();
            field["first_timestamp"] = QString::number(first);
            it.value() = field;
        }
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
        const auto hours = model.hourlyForecast();
        for (int i = 0; i < 3; ++i) {
            const auto cell = hours[i].toMap()["cells"].toMap()["pcpttl_aver"].toMap();
            QCOMPARE(cell, rain);
            QCOMPARE(cell["intervalLabel"].toString(), model.formatTime(first, labelFormat) + "–" + model.formatTime(first + 10800, labelFormat));
            QCOMPARE(hours[i].toMap()["pcpttl_aver"].toDouble(), 1.0);
        }
        const auto missing = hours[6].toMap()["cells"].toMap()["pcpttl_aver"].toMap();
        QVERIFY(!missing["number"].isValid());
        QCOMPARE(missing["value"].toString(), QStringLiteral("—"));
        QCOMPARE(missing["intervalTo"].toLongLong(), first + 32400);
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
            {"locationName", "GPS point"}, {"latitude", latitude}, {"longitude", longitude}, {"hasPoint", true}}));
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
        editor->setProperty("latitude", 1.2345678912345);
        QVERIFY(editor->property("valid").toBool()); // Editing a place is independent of its forecast model.
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
    void externalForecastPreviewDoesNotSaveThePlaceOrChangeTheLastLocation()
    {
        QSettings settings;
        settings.beginGroup("Location");
        QVariantMap original;
        for (const auto &key : settings.childKeys()) original.insert(key, settings.value(key));
        const auto restore = qScopeGuard([&] {
            settings.remove("");
            for (auto it = original.cbegin(); it != original.cend(); ++it) settings.setValue(it.key(), it.value());
            settings.sync();
        });
        settings.setValue("locationName", "Warszawa");
        settings.setValue("latitude", 52.2297);
        settings.setValue("longitude", 21.0122);
        settings.setValue("model", "um4_60");
        settings.sync();
        QQmlEngine engine;
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        QVERIFY(preferences);
        const auto places = preferences->favorites();
        const auto recent = preferences->recentPlaces();
        const QVariantMap preview{{"locationName", "New York"}, {"latitude", 40.7128}, {"longitude", -74.006}, {"model", "gfs"}};
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/apps/meteo/Main.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"smokeTest", true}, {"startupForecast", preview}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *weather = object->findChild<WeatherModel *>();
        auto *locationSettings = object->findChild<QObject *>("locationSettings");
        QVERIFY(weather); QVERIFY(locationSettings);
        QTest::qWait(100); // Allow saved-place synchronization and delayed Settings writes.
        QCOMPARE(weather->locationName(), "New York");
        QCOMPARE(weather->model(), "gfs");
        QVERIFY(object->property("temporaryLocation").toBool());
        QCOMPARE(preferences->favorites(), places);
        QCOMPARE(preferences->recentPlaces(), recent);
        QCOMPARE(locationSettings->property("locationName").toString(), "Warszawa");
        QCOMPARE(locationSettings->property("latitude").toDouble(), 52.2297);
        QCOMPARE(locationSettings->property("longitude").toDouble(), 21.0122);
        QCOMPARE(locationSettings->property("model").toString(), "um4_60");
        // Explicitly choosing a saved place ends the preview and persists that choice.
        QVERIFY(QMetaObject::invokeMethod(object.data(), "selectForecast", Q_ARG(QVariant, "Warszawa"),
            Q_ARG(QVariant, 52.2297), Q_ARG(QVariant, 21.0122), Q_ARG(QVariant, "um4_120")));
        QVERIFY(!object->property("temporaryLocation").toBool());
        QCOMPARE(weather->locationName(), "Warszawa");
        QCOMPARE(weather->model(), "um4_120");
        QCOMPARE(locationSettings->property("model").toString(), "um4_120");
    }
    void addLocationStartsWithAnEmptyDraftAndSavesOnlyOnConfirmation()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/apps/meteo/Main.qml")));
        const QVariantMap preview{{"locationName", "New York"}, {"latitude", 40.7128}, {"longitude", -74.006}, {"model", "gfs"}};
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"smokeTest", true}, {"startupForecast", preview}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *weather = object->findChild<WeatherModel *>();
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        const auto places = preferences->favorites();
        auto *add = object->findChild<QObject *>("addLocationButton");
        auto *dialog = object->findChild<QObject *>("locationDialog");
        QVERIFY(add && dialog);
        auto *editor = dialog->findChild<QObject *>("locationEditor");
        auto *save = dialog->findChild<QObject *>("saveLocationButton");
        QVERIFY(editor && save);
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QTRY_VERIFY(dialog->property("visible").toBool());
        QCOMPARE(editor->property("locationName").toString(), QString());
        QVERIFY(!std::isfinite(editor->property("latitude").toDouble()));
        editor->setProperty("locationName", "Name without a point");
        QVERIFY(!save->property("enabled").toBool());
        const QVariantMap place{{"name", "Test place"}, {"latitude", 1.2345}, {"longitude", 2.3456}};
        QVERIFY(QMetaObject::invokeMethod(editor, "selectPlace", Q_ARG(QVariant, place)));
        QVERIFY(save->property("enabled").toBool());
        QCOMPARE(save->property("text").toString(), "Add");
        QCOMPARE(preferences->favorites(), places);
        QVERIFY(QMetaObject::invokeMethod(dialog, "reject"));
        QCOMPARE(preferences->favorites(), places);
        QVERIFY(QMetaObject::invokeMethod(add, "clicked"));
        QVERIFY(QMetaObject::invokeMethod(editor, "selectPlace", Q_ARG(QVariant, place)));
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        QCOMPARE(weather->locationName(), "Test place");
        const auto saved = preferences->placeAt(1.2345, 2.3456);
        QVERIFY(!saved.isEmpty());
        dialog->setProperty("place", saved);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        editor->setProperty("locationName", "Changed name");
        editor->setProperty("latitude", 1.5);
        QVERIFY(QMetaObject::invokeMethod(dialog, "reject"));
        QCOMPARE(preferences->placeAt(1.2345, 2.3456), saved);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        editor->setProperty("locationName", "Changed name");
        editor->setProperty("latitude", 1.5);
        QVERIFY(QMetaObject::invokeMethod(dialog, "save"));
        const auto moved = preferences->placeAt(1.5, 2.3456);
        QCOMPARE(moved["id"], saved["id"]);
        QCOMPARE(moved["name"].toString(), "Changed name");
        QCOMPARE(preferences->favorites().size(), places.size() + 1);
        QObject *panel = nullptr;
        for (auto *candidate : object->findChildren<QObject *>())
            if (QByteArray(candidate->metaObject()->className()).startsWith("ForecastPanel_")) { panel = candidate; break; }
        QVERIFY(panel);
        QTRY_COMPARE(weather->latitude(), 1.5);
        QVERIFY(preferences->editPlace(saved["id"].toString(), "Changed name", 50.87, 20.63)["ok"].toBool());
        QTRY_COMPARE(weather->latitude(), 50.87);
        QVERIFY(QMetaObject::invokeMethod(panel, "modelSelected", Q_ARG(QString, QStringLiteral("um4_120"))));
        QCOMPARE(weather->model(), "um4_120");
        QCOMPARE(object->findChild<QObject *>("locationSettings")->property("model").toString(), "um4_120");
        preferences->removePlace(50.87, 20.63);
    }
    void locationDialogUsesInsetContentAndFitsNarrowWindows()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*(TypeError|ReferenceError|Binding loop).*")));
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/apps/meteo/Main.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"smokeTest", true}, {"startupForecast", QVariantMap{{"locationName", "Warszawa"}, {"latitude", 52.2297}, {"longitude", 21.0122}, {"model", "um4_60"}}}}));
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
        auto *content = dialog->findChild<QQuickItem *>("locationDialogContent");
        auto *search = dialog->findChild<QQuickItem *>("placeSearchField");
        auto *manual = dialog->findChild<QQuickItem *>("manualLocationToggle");
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
        QTRY_VERIFY(content->implicitHeight() > collapsed);
        QVERIFY(dialog->findChild<QQuickItem *>("manualLatitude")->isVisible());
        QVERIFY(dialog->findChild<QQuickItem *>("manualLongitude")->isVisible());
        for (const auto &name : {"customLocationName", "manualLatitude", "manualLongitude"}) {
            auto *field = dialog->findChild<QQuickItem *>(name);
            QVERIFY(field);
            QTRY_VERIFY2(([&] {
                const auto x = field->mapToItem(content, {0, 0}).x();
                const auto padding = content->property("padding").toDouble();
                return field->width() >= 220 && x >= padding
                    && x + field->width() <= content->width() - padding + 1;
            })(), name);
        }
        manual->setProperty("checked", false);
        QTRY_VERIFY(qAbs(content->implicitHeight() - collapsed) < 2);
        QMetaObject::invokeMethod(dialog, "close");
    }
    void compactWidgetHeaderShowsUsefulFactsWithoutExpanding()
    {
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|Binding loop|Cannot read|Cannot assign).*"));
        WeatherModel weather; weather.setAutoRefresh(false); weather.refresh(false);
        QVERIFY(weather.hasData());
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "CompactForecast");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> header(component.createWithInitialProperties({{"weather", QVariant::fromValue(&weather)}, {"showDays", false}}));
        QVERIFY2(header, qPrintable(component.errorString()));
        QQuickWindow window; window.resize(620, 200);
        auto *item = qobject_cast<QQuickItem *>(header.data());
        item->setParentItem(window.contentItem()); window.show();
        auto *facts = header->findChild<QQuickItem *>("compactForecastFacts");
        auto *sunrise = header->findChild<QQuickItem *>("compactSunrise");
        auto *sunset = header->findChild<QQuickItem *>("compactSunset");
        QVERIFY(facts && sunrise && sunset);
        for (const int width : {620, 400}) {
            item->setSize(QSizeF(width, 150));
            QTRY_VERIFY(item->implicitHeight() <= 150);
            QTRY_VERIFY(facts->isVisible() && sunrise->isVisible() && sunset->isVisible());
            QVERIFY(facts->property("text").toString().contains(weather.windText()));
            QVERIFY(facts->property("text").toString().contains(weather.pressureText()));
            QTRY_VERIFY(facts->mapToItem(item, {facts->width(), 0}).x() <= sunrise->mapToItem(item, {0, 0}).x());
            QVERIFY(sunset->mapToItem(item, {sunset->width(), 0}).x() <= width);
            QCOMPARE(sunrise->property("time").toString(), weather.solarTimes()["sunriseText"].toString());
            QCOMPARE(sunset->property("time").toString(), weather.solarTimes()["sunsetText"].toString());
        }
        weather.setUtc(true);
        QCOMPARE(sunrise->property("time").toString(), weather.solarTimes()["sunriseText"].toString());
        QCOMPARE(sunset->property("time").toString(), weather.solarTimes()["sunsetText"].toString());
        weather.setLatitude(50.8661);
        QVERIFY(!weather.hasData());
        QVERIFY(!facts->isVisible() && !sunrise->isVisible() && !sunset->isVisible());
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
        const auto configured = weather.configuredCharts(defaultChartLayout("classic"));
        QVERIFY(configured.size() > 2);
        const int sectionCount = configured.size();
        QTRY_COMPARE(list->property("count").toInt(), sectionCount);
        QQuickItem *temperature = nullptr, *precipitation = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, temperature), Q_ARG(int, 0)) && temperature);
        QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, precipitation), Q_ARG(int, 1)) && precipitation);
        if (precipitation->metaObject()->indexOfProperty("index") >= 0) QCOMPARE(precipitation->property("index").toInt(), 1);
        QCOMPARE(temperature->objectName(), "widgetSection-temperature");
        QCOMPARE(precipitation->objectName(), "widgetSection-precipitation");
        QVERIFY(temperature->property("expanded").toBool());
        QTRY_VERIFY(temperature->height() > 150);
        QVERIFY(QMetaObject::invokeMethod(temperature, "collapse"));
        QTRY_VERIFY(temperature->height() < 100);
        QVERIFY(QMetaObject::invokeMethod(precipitation, "collapse"));
        QTRY_VERIFY(precipitation->height() < 100);
        QVERIFY(QMetaObject::invokeMethod(precipitation, "expand"));
        QTRY_VERIFY(precipitation->height() > 150);
        QVERIFY(!temperature->property("expanded").toBool());
        QVERIFY(QMetaObject::invokeMethod(temperature, "expand"));
        QTRY_VERIFY(temperature->height() > 150);
        QVERIFY(QMetaObject::invokeMethod(list, "positionViewAtEnd"));
        QQuickItem *last = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, last), Q_ARG(int, sectionCount - 1)) && last);
        QCOMPARE(last->objectName(), "widgetSection-" + configured.last().toMap()["key"].toString());
    }
    void nativeWidgetInstancesKeepLocalChartsAndShareDisplayPreferences()
    {
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|Binding loop|Cannot read|Cannot assign).*"));
        QQmlEngine engine;
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        const auto oldProfile = preferences->profile();
        const auto oldUnit = preferences->windUnit();
        const auto oldUtc = preferences->utc();
        preferences->setProfile("classic");
        const auto firstProfile = preferences->createProfile("First popup");
        preferences->setChartOptions("temperature", {{"title", "First temperature"}, {"height", 130}});
        const auto secondProfile = preferences->createProfile("Second popup");
        preferences->setChartOptions("temperature", {{"title", "Second temperature"}, {"height", 170}});
        preferences->setProfile("basic");
        const auto restore = qScopeGuard([&] {
            for (const auto &id : {firstProfile, secondProfile}) { preferences->setProfile(id); preferences->deleteProfile(); }
            preferences->setProfile(oldProfile); preferences->setWindUnit(oldUnit); preferences->setUtc(oldUtc);
        });
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import pl.meteo.desktop
            import "../integrations/plasma/package/contents/ui" as Widget
            Item {
                required property string firstProfile
                required property string secondProfile
                width: 1100; height: 720
                WeatherModel { id: a; objectName: "firstPopupWeather"; autoRefresh: false; windUnit: UserPreferences.windUnit; utc: UserPreferences.utc }
                WeatherModel { id: b; objectName: "secondPopupWeather"; autoRefresh: false; windUnit: UserPreferences.windUnit; utc: UserPreferences.utc }
                Widget.ForecastPopup { objectName: "firstPopup"; width: 540; height: 720; weather: a; profile: parent.firstProfile; hours: 24; clockTime: a.firstTimestamp + 3600 }
                Widget.ForecastPopup { objectName: "secondPopup"; x: 550; width: 540; height: 720; weather: b; profile: parent.secondProfile; hours: 48; clockTime: b.firstTimestamp + 3600 }
            }
        )", QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/tests/popup-host.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> host(component.createWithInitialProperties({{"firstProfile", firstProfile}, {"secondProfile", secondProfile}}));
        QVERIFY2(host, qPrintable(component.errorString()));
        auto *a = host->findChild<WeatherModel *>("firstPopupWeather");
        auto *b = host->findChild<WeatherModel *>("secondPopupWeather");
        QVERIFY(a && b); a->refresh(false); b->refresh(false);
        QVERIFY(a->hasData() && b->hasData());
        QQuickWindow window; window.resize(1100, 720);
        qobject_cast<QQuickItem *>(host.data())->setParentItem(window.contentItem()); window.show();
        auto *first = host->findChild<QObject *>("firstPopup");
        auto *second = host->findChild<QObject *>("secondPopup");
        auto *selectionA = first->findChild<QObject *>("widgetChartSelection");
        auto *selectionB = second->findChild<QObject *>("widgetChartSelection");
        QVERIFY(selectionA && selectionB);
        const auto span = [](QObject *selection) { return selection->property("endTime").toDouble() - selection->property("startTime").toDouble(); };
        const double secondSpan = std::min(48.0 * 3600, double(b->lastTimestamp() - b->firstTimestamp()));
        QCOMPARE(span(selectionA), 24.0 * 3600); QCOMPARE(span(selectionB), secondSpan);
        const auto charts = [](QObject *selection) {
            const auto value = selection->property("charts");
            return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toList() : value.toList();
        };
        const auto chartsB = charts(selectionB);
        first->setProperty("hours", 12);
        QCOMPARE(span(selectionA), 12.0 * 3600); QCOMPARE(span(selectionB), secondSpan);
        preferences->setProfile(firstProfile);
        preferences->setChartOptions("temperature", {{"title", "Updated first temperature"}});
        preferences->setProfile("basic");
        QTRY_COMPARE(charts(selectionA).first().toMap()["title"].toString(), "Updated first temperature");
        QCOMPARE(charts(selectionB), chartsB);
        QCOMPARE(charts(selectionB).first().toMap()["title"].toString(), "Second temperature");
        auto *sectionsA = first->findChild<QObject *>("widgetSections");
        auto *sectionsB = second->findChild<QObject *>("widgetSections");
        QQuickItem *rowA = nullptr, *rowB = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(sectionsA, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, rowA), Q_ARG(int, 0)) && rowA);
        QTRY_VERIFY(QMetaObject::invokeMethod(sectionsB, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, rowB), Q_ARG(int, 0)) && rowB);
        QSignalSpy toggled(first, SIGNAL(sectionToggled(QString,bool)));
        QVERIFY(QMetaObject::invokeMethod(rowA, "collapse"));
        QTRY_VERIFY(!rowA->property("expanded").toBool());
        QVERIFY(rowB->property("expanded").toBool());
        QVERIFY(!toggled.isEmpty());
        preferences->setWindUnit(oldUnit == "km/h" ? "m/s" : "km/h");
        preferences->setUtc(!oldUtc);
        QTRY_COMPARE(a->windUnit(), preferences->windUnit());
        QTRY_COMPARE(b->windUnit(), preferences->windUnit());
        QCOMPARE(a->utc(), !oldUtc); QCOMPARE(b->utc(), !oldUtc);
        QCOMPARE(first->property("profile").toString(), firstProfile);
        QCOMPARE(second->property("profile").toString(), secondProfile);
        QCOMPARE(span(selectionA), 12.0 * 3600); QCOMPARE(span(selectionB), secondSpan);
        QCOMPARE(preferences->profile(), "basic");
    }
    void nativeWidgetScrollsOnlyWhenContentExceedsAvailableHeight()
    {
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|Binding loop|Cannot read|Cannot assign).*"));
        WeatherModel weather; weather.setAutoRefresh(false); weather.refresh(false);
        QVERIFY(weather.hasData());
        QQmlEngine engine;
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        const auto previous = preferences->profile();
        preferences->setProfile("classic");
        const auto profile = preferences->createProfile("Scroll test");
        const auto restore = qScopeGuard([&] { preferences->setProfile(profile); preferences->deleteProfile(); preferences->setProfile(previous); });
        preferences->setChartOptions("temperature", {{"height", 760}});
        for (const auto &entry : preferences->chartLayout()) {
            const auto key = entry.toMap()["key"].toString();
            if (key != "temperature") preferences->setChartEnabled(key, false);
        }
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/ForecastPopup.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> popup(component.createWithInitialProperties({{"weather", QVariant::fromValue(&weather)}, {"profile", profile}}));
        QVERIFY2(popup, qPrintable(component.errorString()));
        QQuickWindow window; window.resize(540, 1400);
        auto *item = qobject_cast<QQuickItem *>(popup.data());
        item->setParentItem(window.contentItem()); item->setSize(window.size()); window.show();
        auto *list = popup->findChild<QObject *>("widgetSections");
        auto *bar = popup->findChild<QObject *>("widgetVerticalScrollBar");
        QVERIFY(list && bar);
        QTRY_COMPARE(list->property("count").toInt(), 1);
        auto *footer = list->property("footerItem").value<QQuickItem *>();
        QVERIFY(footer && !footer->isVisible());
        QCOMPARE(footer->height(), 0);
        QQuickItem *temperature = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, temperature), Q_ARG(int, 0)) && temperature);
        QTRY_VERIFY(item->implicitHeight() > 720);
        QTRY_VERIFY(!bar->property("visible").toBool());
        item->setHeight(300);
        QTRY_VERIFY(bar->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(temperature, "collapse"));
        QTRY_VERIFY(temperature->height() < 100);
        QTRY_VERIFY(!bar->property("visible").toBool());
        item->setHeight(1400);
        QVERIFY(QMetaObject::invokeMethod(temperature, "expand"));
        QTRY_VERIFY(temperature->height() > 760);
        QTRY_VERIFY(!bar->property("visible").toBool());
    }
    void nativeWidgetHighlightFollowsHoveredSection()
    {
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|Binding loop|Cannot read|Cannot assign).*"));
        WeatherModel weather; weather.setAutoRefresh(false); weather.refresh(false);
        QVERIFY(weather.hasData());
        QStringList collapsed;
        for (const auto &chart : weather.configuredCharts(defaultChartLayout("classic"))) collapsed.append(chart.toMap()["key"].toString());
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/ForecastPopup.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> popup(component.createWithInitialProperties({{"weather", QVariant::fromValue(&weather)}, {"collapsedCharts", collapsed}}));
        QVERIFY2(popup, qPrintable(component.errorString()));
        QQuickWindow window; window.resize(640, 1000);
        auto *item = qobject_cast<QQuickItem *>(popup.data());
        item->setParentItem(window.contentItem()); item->setSize(window.size()); window.show();
        auto *list = popup->findChild<QObject *>("widgetSections");
        QVERIFY(list);
        QTRY_COMPARE(list->property("count").toInt(), collapsed.size());
        QTest::qWait(100);
        for (const int index : {2, 4, 1, 6, 0}) {
            QQuickItem *row = nullptr;
            QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, index)) && row);
            const auto position = row->mapToScene(QPointF(row->width() / 3, row->height() / 2)).toPoint();
            QTest::mouseMove(&window, position);
            QTRY_COMPARE_WITH_TIMEOUT(list->property("currentIndex").toInt(), index, 1000);
            QCOMPARE(list->property("currentItem").value<QQuickItem *>(), row);
        }
        QQuickItem *clicked = nullptr;
        QVERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, clicked), Q_ARG(int, 4)));
        QVERIFY(clicked);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier,
            clicked->mapToScene(QPointF(clicked->width() / 3, clicked->height() / 2)).toPoint());
        QTRY_COMPARE(list->property("currentIndex").toInt(), 4);
        QVERIFY(clicked->property("expanded").toBool());
        clicked->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Down);
        QTRY_COMPARE(list->property("currentIndex").toInt(), 5);
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
        QCOMPARE(order[0], preferences->placeAt(50.8661, 20.6286)["id"].toString());
        QCOMPARE(order[1], preferences->placeAt(54.352, 18.6466)["id"].toString());
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
            {"clockTime", model.firstTimestamp() + 3600.0}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        model.refresh(false); // The popup is created before its forecast arrives.
        QVERIFY(model.hasData());
        const auto prepared = model.configuredCharts(preferences->chartLayoutForProfile(id));
        QVERIFY(prepared.size() > 2);
        const auto chartsValue = object->property("charts");
        const auto widgetCharts = chartsValue.metaType() == QMetaType::fromType<QJSValue>()
            ? chartsValue.value<QJSValue>().toVariant().toList() : chartsValue.toList();
        QCOMPARE(widgetCharts, prepared);
        auto *repeater = object->findChild<QObject *>("widgetChartRepeater");
        QVERIFY(repeater);
        QCOMPARE(repeater->property("count").toInt(), prepared.size());
        auto *root = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        window.resize(540, 660);
        root->setParentItem(window.contentItem()); root->setSize(window.size()); window.show();
        QQuickItem *temperature = nullptr, *wind = nullptr;
        int windIndex = -1;
        for (qsizetype i = 0; i < prepared.size(); ++i) if (prepared[i].toMap()["key"] == "wind") windIndex = i;
        QVERIFY(windIndex >= 0);
        QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Q_RETURN_ARG(QQuickItem *, temperature), Q_ARG(int, 0)));
        QVERIFY(QMetaObject::invokeMethod(repeater, "itemAt", Q_RETURN_ARG(QQuickItem *, wind), Q_ARG(int, windIndex)));
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
        preferences->setProfile(id);
        preferences->setChartEnabled("wind", false);
        preferences->setProfile("basic");
        QTRY_COMPARE(repeater->property("count").toInt(), prepared.size() - 1);
        QCOMPARE(preferences->profile(), "basic");
    }
    void widgetConfigurationUsesAllEnabledProfileCharts()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/configAppearance.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QVERIFY(object->property("isSaveAllowed").toBool());
        QVERIFY(object->property("chartCatalog").value<QJSValue>().toVariant().toList().size() > 2);
        object->setProperty("cfg_chartProfile", "basic");
        QVERIFY(object->property("isSaveAllowed").toBool());
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
        auto *preferences = engine.singletonInstance<UserPreferences *>("pl.meteo.desktop", "UserPreferences");
        preferences->savePlace("Kielce", 50.8661, 20.6286);
        preferences->savePlace("Gdańsk", 54.352, 18.6466);
        const auto available = preferences->favorites();
        const auto indexOf = [&](double latitude) {
            for (int i = 0; i < available.size(); ++i)
                if (qAbs(available[i].toMap()["latitude"].toDouble() - latitude) < 1e-6) return i;
            return -1;
        };
        const auto path = QStringLiteral(METEO_SOURCE_DIR "/integrations/plasma/package/contents/ui/configGeneral.qml");
        QQmlComponent component(&engine, QUrl::fromLocalFile(path));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> editor(component.create());
        QVERIFY2(editor, qPrintable(component.errorString()));
        editor->setProperty("cfg_locationName", "Kielce");
        editor->setProperty("cfg_latitude", 50.8661);
        editor->setProperty("cfg_longitude", 20.6286);
        auto *selector = editor->findChild<QObject *>("widgetLocationSelector");
        auto *model = editor->findChild<QObject *>("widgetForecastModelSelector");
        QVERIFY(selector && model);
        QCOMPARE(selector->property("currentIndex").toInt(), indexOf(50.8661));
        QVERIFY(editor->property("isSaveAllowed").toBool());
        QVERIFY(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, indexOf(54.352))));
        QCOMPARE(editor->property("cfg_locationName").toString(), "Gdańsk");
        QCOMPARE(editor->property("cfg_latitude").toDouble(), 54.352);
        QVERIFY(QMetaObject::invokeMethod(model, "activated", Q_ARG(int, 2)));
        QCOMPARE(editor->property("cfg_model").toString(), "gfs");
        editor->setProperty("cfg_model", "um4_120");
        QCOMPARE(model->property("currentIndex").toInt(), 1);
    }
};
QTEST_MAIN(PresentationTest)
#include "test_presentation.moc"
