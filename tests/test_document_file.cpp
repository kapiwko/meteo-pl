#include "core/DocumentFile.hpp"
#include <QTest>

class DocumentFileTest : public QObject {
    Q_OBJECT
private slots:
    void androidDocumentIdsRemainOpaque()
    {
        const auto url = QUrl::fromEncoded("content://com.android.externalstorage.documents/document/primary%3ADownload%2FWykres%20%C5%82%C3%B3d%C5%BA.png");
        QVERIFY(Meteo::isContentDocument(url));
        QCOMPARE(Meteo::documentPath(url).toUtf8(), url.toEncoded());
        QCOMPARE(Meteo::withDefaultDocumentSuffix(url, "png"), url);
        // Numeric provider IDs need not resemble the selected filename.
        const QUrl numeric("content://com.android.providers.downloads.documents/document/1234");
        QCOMPARE(Meteo::withDefaultDocumentSuffix(numeric, "png"), numeric);
        QCOMPARE(Meteo::withDefaultDocumentSuffix(numeric, "csv"), numeric);
#ifdef Q_OS_ANDROID
        QVERIFY(Meteo::isDocumentFile(url));
#else
        QVERIFY(!Meteo::isDocumentFile(url));
#endif
    }
    void localFilesKeepTheirPathsAndOptionalSuffixes()
    {
        const auto url = QUrl::fromLocalFile("/tmp/Wykres łódź 100%");
        QVERIFY(Meteo::isDocumentFile(url));
        QVERIFY(!Meteo::isContentDocument(url));
        QCOMPARE(Meteo::documentPath(url), QStringLiteral("/tmp/Wykres łódź 100%"));
        const auto png = Meteo::withDefaultDocumentSuffix(url, "png");
        QCOMPARE(png.toLocalFile(), QStringLiteral("/tmp/Wykres łódź 100%.png"));
        QCOMPARE(Meteo::withDefaultDocumentSuffix(png, "png"), png);
        QCOMPARE(Meteo::documentDisplayPath(png), png.toLocalFile());
    }
    void rejectsUnselectedAndRemoteFiles()
    {
        for (const auto &url : {QUrl(), QUrl("relative.png"), QUrl("https://example.org/chart.png"),
                               QUrl("qrc:/chart.png"), QUrl("content:///missing-provider"), QUrl("content://provider")})
            QVERIFY2(!Meteo::isDocumentFile(url), qPrintable(url.toString()));
    }
};
QTEST_GUILESS_MAIN(DocumentFileTest)
#include "test_document_file.moc"
