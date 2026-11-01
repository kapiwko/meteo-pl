#include "network/DiskCacheBudget.hpp"
#include "network/CacheFiles.hpp"
#include <QProcess>
#include <QScopeGuard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

class DiskCacheTest : public QObject {
    Q_OBJECT
private slots:
    void clearingNeverFollowsDirectoryRedirects_data()
    {
        QTest::addColumn<bool>("rootLink");
        QTest::newRow("nested redirect") << false;
        QTest::newRow("redirect as root") << true;
    }
    void clearingNeverFollowsDirectoryRedirects()
    {
        QFETCH(bool, rootLink);
        QTemporaryDir cache, outside;
        const auto keep = outside.filePath("keep.json");
        QFile file(keep); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("external data"); file.close();
        const auto link = cache.filePath("redirect");
#ifdef Q_OS_WIN
        QCOMPARE(QProcess::execute("cmd.exe", {"/D", "/C", "mklink", "/J", QDir::toNativeSeparators(link), QDir::toNativeSeparators(outside.path())}), 0);
        QVERIFY(QFileInfo(link).isJunction());
        const auto cleanup = qScopeGuard([&] { QDir().rmdir(link); });
#else
        QVERIFY(QFile::link(outside.path(), link));
        const auto cleanup = qScopeGuard([&] { QFile::remove(link); });
#endif
        const auto root = rootLink ? link : cache.path();
        qint64 bytes = 0;
        QCOMPARE(Meteo::visitCacheFiles(root, [&](const QFileInfo &entry) { bytes += entry.size(); return true; }), !rootLink);
        QCOMPARE(bytes, 0);
        QCOMPARE(Meteo::clearCacheFiles(root), !rootLink);
        QVERIFY(QFile::exists(keep));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("external data"));
    }
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
        QVERIFY(Meteo::touchCacheEntry(directory.filePath("used.json")));
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
    void touchingNeverCreatesOrTruncatesFiles()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("missing.json");
        QVERIFY(!Meteo::touchCacheEntry(path)); QVERIFY(!QFile::exists(path));
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("keep"); file.close();
        QVERIFY(Meteo::touchCacheEntry(path));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("keep"));
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
