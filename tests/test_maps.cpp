#include "network/ImageCache.hpp"
#include "WeatherModel.hpp"
#include "ImagePreloader.hpp"
#include "ImageServer.hpp"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QJSValue>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QWheelEvent>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTest>

class MapTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_cache;
private slots:
    void initTestCase()
    {
        QVERIFY(m_cache.isValid());
        QCoreApplication::setApplicationName("MeteoMapTest");
        QQuickStyle::setStyle("org.kde.desktop");
        qputenv("XDG_CACHE_HOME", m_cache.path().toUtf8());
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|Binding loop|Cannot assign|Unable to assign).*"));
    }
    void namedPlacesTrackProjectionAndClicksDoNotSelectAnotherPoint()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "TileMap");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const QVariantList places{QVariantMap{{"name", "Warszawa"}, {"latitude", 52.2297}, {"longitude", 21.0122}},
            QVariantMap{{"name", "Kielce"}, {"latitude", 50.8661}, {"longitude", 20.6286}}};
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"latitude", 52.2297}, {"longitude", 21.0122},
            {"markerLatitude", 52.2297}, {"markerLongitude", 21.0122}, {"zoom", 6}, {"active", false}, {"places", places}}));
        QVERIFY(object);
        auto *map = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window; window.resize(640, 400);
        map->setParentItem(window.contentItem()); map->setSize({640, 400}); window.show();
        const auto find = [](auto &&self, QQuickItem *parent, const QString &name) -> QQuickItem * {
            if (parent->objectName() == name) return parent;
            for (auto *child : parent->childItems()) if (auto *found = self(self, child, name)) return found;
            return nullptr;
        };
        auto *current = find(find, map, "map-place-label-0");
        auto *other = find(find, map, "map-place-label-1");
        QVERIFY(current); QVERIFY(other);
        QTRY_VERIFY(current->width() > 0);
        QVERIFY(current->isVisible()); QVERIFY(other->isVisible());
        QVERIFY(current->property("highlighted").toBool());
        QVERIFY(!other->property("highlighted").toBool());
        QSignalSpy selected(object.data(), SIGNAL(placeSelected(QString,double,double)));
        QSignalSpy coordinate(object.data(), SIGNAL(coordinateSelected(double,double)));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, other->mapToScene({other->width()/2, other->height()/2}).toPoint());
        QCOMPARE(selected.count(), 1);
        QCOMPARE(selected.first()[0].toString(), "Kielce");
        QCOMPARE(selected.first()[1].toDouble(), 50.8661);
        QCOMPARE(selected.first()[2].toDouble(), 20.6286);
        QCOMPARE(coordinate.count(), 0);
        const auto before = other->property("coordinatePosition").toPointF();
        object->setProperty("longitude", 22.0122);
        QTRY_VERIFY(other->property("coordinatePosition").toPointF().x() < before.x());
        QVERIFY(qAbs(before.x()-other->property("coordinatePosition").toPointF().x()-256*64/360.0) < 0.01);
        object->setProperty("places", QVariantList{QVariantMap{{"name", "Za południkiem 180°"}, {"latitude", 0}, {"longitude", 179}}});
        object->setProperty("latitude", 0);
        object->setProperty("longitude", -179);
        current = find(find, map, "map-place-label-0");
        QVERIFY(current);
        QTRY_VERIFY(current->isVisible());
        QVERIFY(current->property("coordinatePosition").toPointF().x() > 0);
        QVERIFY(current->property("coordinatePosition").toPointF().x() < 320);
    }
    void cacheDeduplicatesRequestsAndSurvivesRestart()
    {
        ImageServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int completed = 0;
        QString failure;
        const auto receive = [&](const QImage &image, const QString &error) {
            failure = error;
            if (!image.isNull() && image.pixelColor(0, 0) == QColor(Qt::red)) completed++;
        };
        const auto directory = m_cache.path() + "/persistent";
        const auto address = server.url("red");
        {
            Meteo::ImageCache cache(directory);
            cache.request(address, receive);
            cache.request(address, receive);
            QTRY_COMPARE(completed, 2);
            QVERIFY2(failure.isEmpty(), qPrintable(failure));
            QCOMPARE(server.requests.size(), 1);
            cache.request(address, receive);
            QCOMPARE(completed, 3);
            QCOMPARE(server.requests.size(), 1);
        }
        server.close(); // disk cache must work with no server and a new manager
        Meteo::ImageCache reopened(directory);
        reopened.request(address, receive);
        QTRY_COMPARE(completed, 4);
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
        QCOMPARE(server.requests.size(), 1);
    }
    void animationWaitsForEveryImageAndThenReplaysWithoutNetwork()
    {
        ImageServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.hold=true;
        Meteo::ImageCache cache(m_cache.path()+"/preload");
        ImagePreloader loader(nullptr,&cache);
        QVariantList urls;
        for (int i=0;i<9;++i) urls.append(server.url("red/"+QString::number(i)).toString());
        urls.append(urls.first()); // Duplicate background tiles must count once.
        loader.start(urls);
        QTRY_COMPARE(server.held.size(),4);
        QCOMPARE(loader.total(),9);
        QVERIFY(loader.busy());
        QVERIFY(!loader.ready());
        server.releaseOne();
        QTRY_COMPARE(loader.completed(),1);
        QVERIFY(!loader.ready());
        server.hold=false;server.releaseAll();
        QTRY_VERIFY(loader.ready());
        QCOMPARE(loader.completed(),9);
        QCOMPARE(server.requests.size(),9);
        server.close();
        cache.clearMemory(); // Even evicted frames must remain in the disk cache.
        loader.start(urls);
        QTRY_VERIFY(loader.ready());
        QCOMPARE(server.requests.size(),9);
        QVERIFY(loader.error().isEmpty());
    }
    void cancellingAnimationPreloadIgnoresObsoleteCompletions()
    {
        ImageServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.hold=true;
        Meteo::ImageCache cache(m_cache.path()+"/cancel-preload");
        ImagePreloader loader(nullptr,&cache);
        loader.start({server.url("red/old-1"),server.url("red/old-2")});
        QTRY_COMPARE(server.held.size(),2);
        loader.cancel();
        QVERIFY(!loader.busy());QVERIFY(!loader.ready());
        loader.start({server.url("blue/new")});
        QTRY_COMPARE(server.held.size(),3);
        server.releaseOne();server.releaseOne();
        QTest::qWait(60);
        QCOMPARE(loader.completed(),0);
        QVERIFY(!loader.ready());
        server.releaseAll();
        QTRY_VERIFY(loader.ready());
        QCOMPARE(loader.total(),1);
        server.hold=false;
        loader.start({server.url("error")});
        QTRY_VERIFY(!loader.busy());
        QVERIFY(!loader.ready());QVERIFY(!loader.error().isEmpty());
    }
    void thematicImageFitsBothDimensionsUntilUserZooms()
    {
        ImageServer server;
        server.imageSize={1600,900};
        QVERIFY(server.listen(QHostAddress::LocalHost));
        WeatherModel weather;weather.setAutoRefresh(false);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop","ThematicMaps");
        QVERIFY2(component.isReady(),qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"weather",QVariant::fromValue(&weather)}}));
        QVERIFY2(object,qPrintable(component.errorString()));
        object->setProperty("run",0);
        auto *item=qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;window.resize(640,600);item->setParentItem(window.contentItem());item->setSize({640,600});window.show();
        auto *picture=object->findChild<QQuickItem *>("thematicPicture");
        QVERIFY(picture);
        picture->setProperty("source",server.url("red"));
        QTRY_COMPARE(picture->property("displayedSource").toUrl(),server.url("red"));
        const auto fits=[&] { auto *viewport=object->findChild<QQuickItem *>("thematicViewport");return viewport && picture->width()<=viewport->width()+1 && picture->height()<=viewport->height()+1; };
        QTRY_VERIFY(fits());
        QVERIFY(qAbs(picture->width()/picture->height()-1600./900)<0.01);
        object->setProperty("imageScale",2);
        QTRY_VERIFY(!fits());
        object->setProperty("imageScale",1);
        QTRY_VERIFY(fits());
        window.resize(400,800);item->setSize({400,800});
        QTRY_VERIFY(fits());
    }
    void coordinatesRoundTripAndDraggingStaysNearTheLocation()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "TileMap");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"active", false}, {"zoom", 6},
            {"latitude", 50.8661}, {"longitude", 20.6286}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        window.resize(400, 300);
        item->setParentItem(window.contentItem());
        item->setSize({400, 300});
        window.show();
        QVariant result;
        QVERIFY(QMetaObject::invokeMethod(object.data(), "atPoint", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 200), Q_ARG(QVariant, 150)));
        const auto coordinate = result.value<QJSValue>().toVariant().toMap();
        QVERIFY(std::abs(coordinate["latitude"].toDouble() - 50.8661) < 0.000001);
        QVERIFY2(std::abs(coordinate["longitude"].toDouble() - 20.6286) < 0.000001,
            qPrintable(QStringLiteral("Longitude: %1").arg(coordinate["longitude"].toDouble(), 0, 'f', 6)));
        for (const double longitude : {-179.9, -90.0, 0.0, 90.0, 179.9}) {
            object->setProperty("longitude", longitude);
            QVERIFY(QMetaObject::invokeMethod(object.data(), "atPoint", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 200), Q_ARG(QVariant, 150)));
            QVERIFY(std::abs(result.value<QJSValue>().toVariant().toMap()["longitude"].toDouble() - longitude) < 0.000001);
        }
        object->setProperty("longitude", 20.6286);
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, {200, 150});
        QTest::mouseMove(&window, {220, 150}, 20);
        QTest::mouseMove(&window, {240, 150}, 20);
        QTest::mouseMove(&window, {260, 150}, 20);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, {260, 150});
        QVERIFY(object->property("longitude").toDouble() < 20.6286);
        QVERIFY(object->property("longitude").toDouble() > 18.0);
        QVERIFY(std::abs(object->property("latitude").toDouble() - 50.8661) < 0.001);
        QVERIFY(QMetaObject::invokeMethod(object.data(), "atPoint", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 90), Q_ARG(QVariant, 80)));
        const auto beforeZoom = result.value<QJSValue>().toVariant().toMap();
        QVERIFY(QMetaObject::invokeMethod(object.data(), "zoomAt", Q_ARG(QVariant, 1), Q_ARG(QVariant, 90), Q_ARG(QVariant, 80)));
        QVERIFY(QMetaObject::invokeMethod(object.data(), "atPoint", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 90), Q_ARG(QVariant, 80)));
        const auto afterZoom = result.value<QJSValue>().toVariant().toMap();
        QVERIFY(std::abs(afterZoom["longitude"].toDouble() - beforeZoom["longitude"].toDouble()) < 0.000001);
        QVERIFY(std::abs(afterZoom["latitude"].toDouble() - beforeZoom["latitude"].toDouble()) < 0.000001);
    }
    void mouseWheelAndTouchpadZoomTheMap()
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "TileMap");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"active", false}, {"zoom", 6}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        window.resize(400, 300);
        item->setParentItem(window.contentItem());
        item->setSize({400, 300});
        window.show();
        QTest::qWait(30);
        QTest::mouseMove(&window, {90, 80});
        const QPointF cursor(90, 80);
        QWheelEvent wheelIn(cursor, window.mapToGlobal(cursor.toPoint()), {}, {0, 120}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&window, &wheelIn);
        QCOMPARE(object->property("zoom").toInt(), 7);
        QWheelEvent wheelOut(cursor, window.mapToGlobal(cursor.toPoint()), {}, {0, -120}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&window, &wheelOut);
        QCOMPARE(object->property("zoom").toInt(), 6);
        QPointingDevice touchpad("Test touchpad", 444, QInputDevice::DeviceType::TouchPad,
            QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position | QInputDevice::Capability::PixelScroll, 2, 0);
        QWheelEvent touchpadIn(cursor, window.mapToGlobal(cursor.toPoint()), {0, 40}, {}, Qt::NoButton, Qt::NoModifier,
            Qt::ScrollUpdate, false, Qt::MouseEventNotSynthesized, &touchpad);
        QCoreApplication::sendEvent(&window, &touchpadIn);
        QCOMPARE(object->property("zoom").toInt(), 7);
        QWheelEvent touchpadOut(cursor, window.mapToGlobal(cursor.toPoint()), {0, -40}, {}, Qt::NoButton, Qt::NoModifier,
            Qt::ScrollUpdate, false, Qt::MouseEventNotSynthesized, &touchpad);
        QCoreApplication::sendEvent(&window, &touchpadOut);
        QCOMPARE(object->property("zoom").toInt(), 6);
        QWheelEvent touchpadEnd(cursor, window.mapToGlobal(cursor.toPoint()), {}, {}, Qt::NoButton, Qt::NoModifier,
            Qt::ScrollEnd, false, Qt::MouseEventNotSynthesized, &touchpad);
        QCoreApplication::sendEvent(&window, &touchpadEnd);

        // Thematic maps put a Flickable under the wheel handler; verify that
        // it cannot consume the event before the image's zoom handles it.
        WeatherModel weather;
        weather.setAutoRefresh(false);
        QQmlComponent thematicComponent(&engine);
        thematicComponent.loadFromModule("pl.meteo.desktop", "ThematicMaps");
        QVERIFY2(thematicComponent.isReady(), qPrintable(thematicComponent.errorString()));
        QScopedPointer<QObject> thematic(thematicComponent.createWithInitialProperties({{"weather", QVariant::fromValue(&weather)}}));
        QVERIFY2(thematic, qPrintable(thematicComponent.errorString()));
        thematic->setProperty("run", 0); // no external HTTP in this test
        auto *thematicItem = qobject_cast<QQuickItem *>(thematic.data());
        thematicItem->setParentItem(window.contentItem());
        thematicItem->setSize({400, 300});
        QTest::qWait(50);
        auto *viewport = thematic->findChild<QQuickItem *>("thematicViewport");
        QVERIFY(viewport);
        const auto position = viewport->mapToScene({viewport->width() / 2, viewport->height() / 2});
        QTest::mouseMove(&window, position.toPoint());
        QWheelEvent thematicIn(position, window.mapToGlobal(position.toPoint()), {}, {0, 120}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&window, &thematicIn);
        QCOMPARE(thematic->property("imageScale").toDouble(), 1.25);
        QWheelEvent thematicOut(position, window.mapToGlobal(position.toPoint()), {0, -40}, {}, Qt::NoButton, Qt::NoModifier,
            Qt::ScrollBegin, false, Qt::MouseEventNotSynthesized, &touchpad);
        QCoreApplication::sendEvent(&window, &thematicOut);
        QCOMPARE(thematic->property("imageScale").toDouble(), 1.0);
    }
    void keepsCompleteMapUntilEveryReplacementTileIsReady()
    {
        ImageServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "TileMap");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const auto tileUrl = [&](const QString &color) { return server.url(color).toString() + "/{z}/{x}/{y}"; };
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"baseTileTemplate", tileUrl("red")}, {"zoom", 3}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        window.resize(400, 300);
        item->setParentItem(window.contentItem());
        item->setSize({400, 300});
        window.show();
        QTRY_VERIFY(object->property("hasFrame").toBool());
        QTRY_VERIFY(!object->property("loading").toBool());
        QTRY_COMPARE(window.grabWindow().pixelColor(80, 100), QColor(Qt::red));
        auto *original = object->property("front").value<QObject *>();
        QVERIFY(original);
        const auto originalX = original->property("x").toDouble();
        // Old tiles move using their geographical coordinates, rather than
        // being reused at unrelated positions while new tiles are loading.
        object->setProperty("longitude", object->property("longitude").toDouble() + 0.1);
        QVERIFY(original->property("x").toDouble() < originalX);
        server.hold = true;
        object->setProperty("baseTileTemplate", tileUrl("blue"));
        QTRY_VERIFY(server.held.size() >= 4);
        QVERIFY(object->property("loading").toBool());
        QCOMPARE(object->property("front").value<QObject *>(), original);
        server.releaseOne();
        QTest::qWait(100);
        QCOMPARE(window.grabWindow().pixelColor(80, 100), QColor(Qt::red));
        QCOMPARE(object->property("front").value<QObject *>(), original);
        server.hold = false;
        server.releaseAll();
        QTRY_VERIFY(!object->property("loading").toBool());
        QVERIFY(object->property("front").value<QObject *>() != original);
        QTRY_COMPARE(window.grabWindow().pixelColor(80, 100), QColor(Qt::blue));
        // Scrubbing back reuses decoded tiles without another HTTP request.
        const auto count = server.requests.size();
        object->setProperty("baseTileTemplate", tileUrl("red"));
        QTRY_COMPARE(window.grabWindow().pixelColor(80, 100), QColor(Qt::red));
        QCOMPARE(server.requests.size(), count);
        object->setProperty("baseTileTemplate", tileUrl("error"));
        QTRY_VERIFY(!object->property("error").toString().isEmpty());
        QCOMPARE(window.grabWindow().pixelColor(80, 100), QColor(Qt::red));
        QVERIFY(object->property("hasFrame").toBool());
    }
    void thematicImageIgnoresObsoleteLoadsAndKeepsImageAfterFailure()
    {
        ImageServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.loadFromModule("pl.meteo.desktop", "BufferedImage");
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"source", server.url("red")}}));
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(object.data());
        QQuickWindow window;
        window.resize(200, 200);
        item->setParentItem(window.contentItem());
        item->setSize({200, 200});
        window.show();
        QTRY_COMPARE(object->property("displayedSource").toUrl(), server.url("red"));
        server.hold = true;
        object->setProperty("source", server.url("blue"));
        QTRY_COMPARE(server.held.size(), 1);
        QCOMPARE(window.grabWindow().pixelColor(40, 40), QColor(Qt::red));
        object->setProperty("source", server.url("green"));
        QTRY_COMPARE(server.held.size(), 2);
        server.releaseOne();
        QTest::qWait(100);
        QCOMPARE(object->property("displayedSource").toUrl(), server.url("red"));
        server.releaseAll();
        QTRY_COMPARE(object->property("displayedSource").toUrl(), server.url("green"));
        server.hold = false;
        object->setProperty("source", server.url("error"));
        QTRY_VERIFY(!object->property("error").toString().isEmpty());
        QCOMPARE(window.grabWindow().pixelColor(40, 40), QColor(Qt::green));
    }
};
QTEST_MAIN(MapTest)
#include "test_maps.moc"
