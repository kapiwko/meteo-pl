#include "network/ImageCache.hpp"
#include "ImageServer.hpp"
#include "ImagePreloader.hpp"
#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class ImageCacheTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_storage;
private slots:
    void initTestCase()
    {
        QVERIFY(m_storage.isValid());
        qputenv("XDG_CACHE_HOME",m_storage.path().toUtf8());
    }
    void globalQueuePrioritizesVisibleImagesAndKeepsSharedConsumers()
    {
        QTemporaryDir directory;
        ImageServer server; server.hold = true;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Meteo::ImageCache cache(directory.path());
        QObject preload, sharedPreload, visible;
        int canceled = 0, completed = 0;
        cache.request(server.url("shared"),[&](const QImage &,const QString &) { ++canceled; },&sharedPreload,Meteo::ImageCache::Priority::Prefetch);
        cache.request(server.url("shared"),[&](const QImage &image,const QString &error) { if (!image.isNull() && error.isEmpty()) ++completed; },&visible);
        for (int i=0;i<10;++i)
            cache.request(server.url(QStringLiteral("prefetch-%1").arg(i)),[](const QImage &,const QString &) {},&preload,Meteo::ImageCache::Priority::Prefetch);
        cache.request(server.url("green-visible"),[&](const QImage &,const QString &) { ++completed; },&visible);
        QTRY_COMPARE(server.requests.size(),4);
        QCOMPARE(cache.activeJobs(),4);
        cache.cancelRequests(&sharedPreload);
        server.releaseOne();
        QTRY_COMPARE(server.requests.size(),5);
        QCOMPARE(server.requests.last().path(),"/green-visible");
        cache.cancelRequests(&preload);
        QTest::qWait(50);
        QCOMPARE(canceled,0);
        QCOMPARE(completed,1);
        QVERIFY(cache.activeJobs() <= 4);
        server.hold = false; server.releaseAll();
        QTRY_COMPARE(completed,2);
        QTRY_COMPARE(cache.activeJobs(),0);
        QCOMPARE(cache.queuedJobs(),0);
    }
    void animationRestartsCancelOrphansWithinTheGlobalBudget()
    {
        QTemporaryDir directory;
        ImageServer server; server.hold = true;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Meteo::ImageCache cache(directory.path());
        ImagePreloader preloader(nullptr,&cache);
        for (int generation=0;generation<8;++generation) {
            QVariantList urls;
            for (int i=0;i<20;++i) urls.append(server.url(QStringLiteral("frame-%1-%2").arg(generation).arg(i)).toString());
            preloader.start(urls);
            QTRY_VERIFY(server.requests.size() >= (generation+1)*4);
            QVERIFY(cache.activeJobs() <= 4);
            QTest::qWait(15);
            int connected = 0;
            for (const auto &request : server.held)
                if (request.socket && request.socket->state() == QAbstractSocket::ConnectedState) ++connected;
            QVERIFY(connected <= 4);
        }
        preloader.cancel();
        QTRY_COMPARE(cache.activeJobs(),0);
        QCOMPARE(cache.queuedJobs(),0);
        QObject batch;
        int rejected = 0;
        for (int i=0;i<300;++i)
            cache.request(server.url(QStringLiteral("bounded-%1").arg(i)),[&](const QImage &,const QString &error) { if (!error.isEmpty()) ++rejected; },&batch);
        QVERIFY(rejected > 0);
        QVERIFY(cache.queuedJobs() <= 252);
        cache.cancelRequests(&batch);
        QTRY_COMPARE(cache.activeJobs(),0);
        QCOMPARE(cache.queuedJobs(),0);
    }
    void separateProcessesCannotOpenTheSameDiskCache()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        qputenv("XDG_CACHE_HOME", directory.path().toUtf8());
        const auto path = directory.filePath("images");
        ImageServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        {
            Meteo::ImageCache owner(path);
            QVERIFY(owner.diskCacheEnabled());
            bool ready = false;
            owner.request(server.url("red"), [&](const QImage &image, const QString &error) { ready = !image.isNull() && error.isEmpty(); });
            QTRY_VERIFY(ready);
            QCOMPARE(server.requests.size(),1);
            QProcess fallback;
            fallback.start(QCoreApplication::applicationFilePath(), {"--cache-probe",path,server.url("red").toString(),"0"});
            QVERIFY(fallback.waitForStarted());
            QTRY_COMPARE(fallback.state(),QProcess::NotRunning);
            QCOMPARE(fallback.exitCode(),0);
            QCOMPARE(server.requests.size(),2); // The fallback cannot read the owner's disk cache.
            QVERIFY(owner.diskCacheEnabled());
        }
        QProcess restarted;
        restarted.start(QCoreApplication::applicationFilePath(), {"--cache-probe",path,server.url("red").toString(),"1"});
        QVERIFY(restarted.waitForStarted());
        QTRY_COMPARE(restarted.state(),QProcess::NotRunning);
        QCOMPARE(restarted.exitCode(),0);
        QCOMPARE(server.requests.size(),2); // A later owner reuses the persistent entry.
    }
};

int main(int argc, char **argv)
{
    QCoreApplication application(argc,argv);
    const auto args = application.arguments();
    if (args.value(1) == "--cache-probe") {
        Meteo::ImageCache cache(args.value(2));
        if (cache.diskCacheEnabled() != (args.value(4) == "1")) return 2;
        const QUrl url(args.value(3));
        cache.request(url,[&](const QImage &image,const QString &error) {
            if (image.isNull() || !error.isEmpty()) { application.exit(3); return; }
            cache.request(url,[&](const QImage &second,const QString &failure) { application.exit(second.isNull() || !failure.isEmpty() ? 4 : 0); });
        });
        QTimer::singleShot(5000,&application,[&] { application.exit(5); });
        return application.exec();
    }
    ImageCacheTest test;
    return QTest::qExec(&test,argc,argv);
}
#include "test_image_cache.moc"
