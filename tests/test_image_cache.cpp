#include "network/ImageCache.hpp"
#include "ImageServer.hpp"
#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class ImageCacheTest : public QObject {
    Q_OBJECT
private slots:
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
