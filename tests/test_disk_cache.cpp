#include "network/DiskCacheBudget.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

class DiskCacheTest : public QObject {
    Q_OBJECT
private slots:
    void retainsTheCurrentAndRecentlyUsedEntriesWithinBothLimits()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto now = QDateTime::currentDateTimeUtc();
        const auto write = [&](const QString &name,int age,int size) {
            QFile file(directory.filePath(name));
            return file.open(QIODevice::WriteOnly) && file.write(QByteArray(size,'x')) == size && file.flush()
                && file.setFileTime(now.addSecs(-age),QFileDevice::FileModificationTime);
        };
        QVERIFY(write("used.json",300,20));
        QVERIFY(write("current.json",200,20));
        QVERIFY(write("unused.json",100,20));
        QVERIFY(write("settings.ini",0,100));
        QVERIFY(QFileInfo(directory.filePath("unused.json")).lastModified() < now.addSecs(-50));
        Meteo::touchCacheEntry(directory.filePath("used.json"));
        QVERIFY(QFileInfo(directory.filePath("used.json")).lastModified() >= now.addSecs(-1));
        Meteo::pruneDiskCache(directory.path(),{"*.json"},{40,2},directory.filePath("current.json"));
        QVERIFY(QFile::exists(directory.filePath("used.json")));
        QVERIFY(QFile::exists(directory.filePath("current.json")));
        QVERIFY(!QFile::exists(directory.filePath("unused.json")));
        QVERIFY(QFile::exists(directory.filePath("settings.ini")));
        Meteo::pruneDiskCache(directory.path(),{"*.json"},{40,1},directory.filePath("current.json"));
        QVERIFY(!QFile::exists(directory.filePath("used.json")));
        QVERIFY(QFile::exists(directory.filePath("current.json")));
    }
    void neverTraversesOtherDirectoriesOrSymbolicLinks()
    {
        QTemporaryDir directory, outside;
        QFile target(outside.filePath("forecast.json"));
        QVERIFY(target.open(QIODevice::WriteOnly)); target.write("keep"); target.close();
        QVERIFY(QDir().mkpath(directory.filePath("other")));
        QFile child(directory.filePath("other/keep.json"));
        QVERIFY(child.open(QIODevice::WriteOnly)); child.write("keep"); child.close();
#ifndef Q_OS_WIN
        QVERIFY(QFile::link(target.fileName(),directory.filePath("link.json")));
#endif
        Meteo::pruneDiskCache(directory.path(),{"*.json"},{0,0},{});
        QVERIFY(QFile::exists(target.fileName()));
        QVERIFY(QFile::exists(child.fileName()));
    }
};
QTEST_GUILESS_MAIN(DiskCacheTest)
#include "test_disk_cache.moc"
