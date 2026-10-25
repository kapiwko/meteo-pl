#include "WeatherModel.hpp"
#include "UserPreferences.hpp"
#include "GpuPlot.hpp"
#include "ForecastFields.hpp"
#include "Fixture.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <cmath>

class RendererTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_data;
    QStringList m_groups;
    static int distance(QRgb a, QRgb b)
    {
        return std::max({std::abs(qRed(a)-qRed(b)), std::abs(qGreen(a)-qGreen(b)), std::abs(qBlue(a)-qBlue(b))});
    }
    static int chroma(QRgb color) { return std::max({qRed(color),qGreen(color),qBlue(color)})-std::min({qRed(color),qGreen(color),qBlue(color)}); }
    static QRgb hue(QRgb color) {
        const auto low=std::min({qRed(color),qGreen(color),qBlue(color)});
        const auto span=std::max(1,chroma(color));
        return qRgb((qRed(color)-low)*255/span,(qGreen(color)-low)*255/span,(qBlue(color)-low)*255/span);
    }
    static int nearest(const QImage &image, int x, int y, QRgb color)
    {
        int result = 255;
        for (int dy=-1; dy<=1; ++dy) for (int dx=-1; dx<=1; ++dx)
            if (image.rect().contains(x+dx,y+dy)) {
                const auto candidate=image.pixel(x+dx,y+dy);
                result=std::min(result,distance(color,candidate));
                // Coverage at an antialiased edge changes brightness, not hue.
                if (chroma(color)>32 && chroma(candidate)>32 && distance(hue(color),hue(candidate))<24) result=0;
            }
        return result;
    }
    struct Difference { double mean=0, unmatched=0, colorUnmatched=0; qint64 coloredMismatchPixels=0; QImage image; };
    static Difference compare(const QImage &cpu, const QImage &gpu)
    {
        Difference result;
        result.image=QImage(cpu.size(),QImage::Format_RGB32);
        qint64 total=0, unmatched=0, colored=0, colorUnmatched=0;
        for (int y=0;y<cpu.height();++y) for (int x=0;x<cpu.width();++x) {
            const auto a=cpu.pixel(x,y),b=gpu.pixel(x,y);
            const int direct=distance(a,b);
            total+=direct;
            result.image.setPixel(x,y,qRgb(direct,direct,direct));
            const auto distinct=[](QRgb p) { return std::max({qRed(p),qGreen(p),qBlue(p)})-std::min({qRed(p),qGreen(p),qBlue(p)})>32; };
            const bool ac=distinct(a),bc=distinct(b);
            const bool different=nearest(gpu,x,y,a)>40 || nearest(cpu,x,y,b)>40;
            if (different) ++unmatched;
            if (ac || bc) { ++colored; if ((ac && nearest(gpu,x,y,a)>40) || (bc && nearest(cpu,x,y,b)>40)) ++colorUnmatched; }
        }
        const auto pixels=cpu.width()*cpu.height();
        result.mean=double(total)/(255*pixels);
        result.unmatched=double(unmatched)/pixels;
        result.colorUnmatched=colored ? double(colorUnmatched)/colored : 0;
        result.coloredMismatchPixels=colorUnmatched;
        return result;
    }
private slots:
    void initTestCase();
    void appearanceMatches_data();
    void appearanceMatches();
    void rejectsMissingGeometryAndChangedColors();
};

void RendererTest::initTestCase()
    {
        QTest::failOnWarning(QRegularExpression(".*(TypeError|ReferenceError|Binding loop|Cannot assign|Unable to assign).*"));
        QQuickStyle::setStyle("org.kde.desktop");
        QVERIFY(m_data.isValid());
        QCoreApplication::setOrganizationName("MeteoRendererTest");
        QCoreApplication::setApplicationName("MeteoRendererTest");
        qputenv("XDG_CACHE_HOME",m_data.path().toUtf8());
        qputenv("XDG_CONFIG_HOME",(m_data.path()+"/config").toUtf8());
        auto document=QJsonDocument::fromJson(forecastFixture()).object();
        auto fields=document["data"].toObject();
        const auto sample=fields["airtmp_point"].toObject();
        for (const auto &field:forecastFields()) {
            auto value=fields.value(field.key).toObject();
            if (value.isEmpty()) value=sample;
            QJsonArray points;
            for (int i=0;i<120;++i) {
                double n=0;
                const double day=std::sin((i-6)*2*3.141592653589793/24);
                if (field.group=="temperature" || field.group=="ground") {
                    n=2+6*day;
                    if (field.key.endsWith("_min")) n-=2;
                    if (field.key.endsWith("_max")) n+=3;
                    if (field.key.startsWith("wchill")) n-=3;
                    if (field.key=="dwptmp_point") n-=4;
                } else if (field.key=="wind10_dr_deg_true_prev_point") n=std::fmod(33.3+i*22.7,360.0);
                else if (field.key=="wind10_sd_true_prev_point") n=3+2*day;
                else if (field.key=="wind_gust_max") n=9+8*day;
                else if (field.key=="pcpttl_type_max") n=i%4;
                else if (field.key.startsWith("pcpttlprob")) n=50+50*day;
                else if (field.key.startsWith("pcpttl")) n=i%24<9 ? (field.key.endsWith("max") ? 12 : 3)*(1+day) : 0;
                else if (field.key.contains("pres")) n=100000+2000*day;
                else if (field.key=="realhum_aver") n=60+35*day;
                else if (field.key=="visibl_min") n=1000+80000*(1+day)/2;
                else if (field.key=="fog_max") n=std::max(0.,day)*0.7;
                else if (field.key=="storm_max") n=i%24<6 ? 1 : 0;
                else if (field.key=="flash_max") n=i%24<6 ? 15 : 0;
                else if (field.key.startsWith("cldbse") || field.key=="cldtop") {
                    n=field.key=="cldtop" ? 10000+5000*day : 1000+800*day+(field.key=="cldbse79" ? 3000 : field.key=="cldbse65" ? 2000 : field.key=="cldbse45" ? 1200 : 0);
                    value["unit"]="m";
                } else n=std::clamp((1+day)/2+(field.key=="cldhigh_aver" ? 0.3 : field.key=="cldmed_aver" ? 0.1 : field.key=="cldvlow_aver" ? -0.3 : -0.1),0.,1.);
                // A gap, including paired ranges, must be preserved by both renderers.
                points.append(i==35 || i==36 ? QJsonValue() : QJsonValue(n));
            }
            value["data"]=points; fields[field.key]=value;
        }
        document["data"]=fields;
        const auto hash=QCryptographicHash::hash("um4_60:52.229700:21.012200",QCryptographicHash::Sha256).toHex();
        QVERIFY(QDir().mkpath(m_data.path()+"/meteo-pl"));
        QFile cache(m_data.path()+"/meteo-pl/"+hash+".json");
        QVERIFY(cache.open(QIODevice::WriteOnly));
        cache.write(QJsonDocument(QJsonObject{{"model","um4_60"},{"latitude",52.2297},{"longitude",21.0122},
            {"savedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"payload",QString::fromUtf8(QJsonDocument(document).toJson())}}).toJson());
        for (const auto &group:forecastCharts()) m_groups.append(group.key);
    }
void RendererTest::appearanceMatches_data()
    {
        QTest::addColumn<QString>("group"); QTest::addColumn<int>("width"); QTest::addColumn<bool>("dark"); QTest::addColumn<bool>("zoomed");
        for (const auto &group:m_groups) {
            QTest::newRow(qPrintable(group+"-wide")) << group << 900 << false << false;
            QTest::newRow(qPrintable(group+"-narrow-zoom")) << group << 440 << false << true;
            QTest::newRow(qPrintable(group+"-dark")) << group << 900 << true << false;
        }
    }
void RendererTest::appearanceMatches()
    {
        QFETCH(QString,group); QFETCH(int,width); QFETCH(bool,dark); QFETCH(bool,zoomed);
        WeatherModel weather; weather.setAutoRefresh(false); weather.refresh(false);
        QVERIFY(weather.hasData());
        QVariantMap definition;
        for (const auto &value:weather.chartGroups()) if (value.toMap()["key"]==group) definition=value.toMap();
        QVERIFY(!definition.isEmpty());
        QQmlEngine engine;
        auto *preferences=engine.singletonInstance<UserPreferences *>("pl.meteo.desktop","UserPreferences");
        QVERIFY(preferences);
        preferences->setShowLegend(true); preferences->setRenderer("cpu");
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import pl.meteo.desktop
            import org.kde.kirigami as Kirigami
            Window {
                id: root
                required property WeatherModel weather
                required property var definition
                required property bool dark
                required property bool zoomed
                Kirigami.Theme.inherit: false
                Kirigami.Theme.backgroundColor: dark ? "#181818" : "#eff0f1"
                Kirigami.Theme.textColor: dark ? "#eff0f1" : "#232629"
                Kirigami.Theme.disabledTextColor: dark ? "#888888" : "#777777"
                color: Kirigami.Theme.backgroundColor
                height: chart.implicitHeight + 16
                visible: true
                Meteogram {
                    id: chart
                    objectName: "parityChart"
                    Kirigami.Theme.inherit: false
                    Kirigami.Theme.backgroundColor: root.dark ? "#181818" : "#eff0f1"
                    Kirigami.Theme.textColor: root.dark ? "#eff0f1" : "#232629"
                    Kirigami.Theme.disabledTextColor: root.dark ? "#888888" : "#777777"
                    x: 8; y: 8; width: root.width - 16
                    weather: root.weather
                    title: root.definition.title; groupKey: root.definition.key
                    fields: root.definition.fields; chartConfiguration: root.definition
                    startTime: root.weather.firstTimestamp + (root.zoomed ? 30*3600 : 0)
                    endTime: root.zoomed ? startTime + 24*3600 : root.weather.lastTimestamp
                    nights: root.weather.nightIntervals
                    nowTime: 0
                    exportMode: !root.zoomed
                    selectedTime: root.zoomed ? startTime + 10*3600 : 0
                }
            }
        )",QUrl());
        QScopedPointer<QObject> object(component.createWithInitialProperties({{"weather",QVariant::fromValue(&weather)},
            {"definition",definition},{"dark",dark},{"zoomed",zoomed},{"width",width}}));
        QVERIFY2(object,qPrintable(component.errorString()));
        auto *window=qobject_cast<QQuickWindow *>(object.data()); QVERIFY(window);
        auto *chart=object->findChild<QObject *>("parityChart"); QVERIFY(chart);
        QCOMPARE(chart->property("darkPalette").toBool(),dark);
        auto *canvas=object->findChild<QObject *>("cpu-base-"+group);
        QVERIFY(canvas);
        QSignalSpy painted(canvas,SIGNAL(painted()));
        QVERIFY(QMetaObject::invokeMethod(canvas,"requestPaint"));
        QTRY_VERIFY(painted.count()>0);
        QTest::qWait(30);
        const auto cpu=window->grabWindow().convertToFormat(QImage::Format_RGB32);
        QVERIFY(!cpu.isNull());
        preferences->setRenderer("gpu");
        QTRY_VERIFY(object->findChild<GpuPlot *>("gpu-base-"+group));
        auto *gpuPlot=object->findChild<GpuPlot *>("gpu-base-"+group);
        QTRY_VERIFY(gpuPlot->frameCount()>0);
        QVERIFY2(window->rendererInterface()->graphicsApi()!=QSGRendererInterface::Software,"Parity tests require the RHI renderer; CPU fallback would hide GPU bugs");
        QTest::qWait(80);
        const auto gpu=window->grabWindow().convertToFormat(QImage::Format_RGB32);
        QCOMPARE(cpu.size(),gpu.size());
        const auto difference=compare(cpu,gpu);
        const bool match=difference.mean<0.015 && difference.unmatched<0.012 && (difference.colorUnmatched<0.025 || difference.coloredMismatchPixels<=64);
        const auto configured=qEnvironmentVariable("METEO_RENDER_ARTIFACTS");
        const auto directory=configured.isEmpty() ? QDir::tempPath()+"/meteo-renderer-diffs" : configured;
        if (!match || !configured.isEmpty()) {
            QVERIFY(QDir().mkpath(directory));
            const auto base=directory+"/"+QString::fromLatin1(QTest::currentDataTag());
            QVERIFY(cpu.save(base+"-cpu.png")); QVERIFY(gpu.save(base+"-gpu.png")); QVERIFY(difference.image.save(base+"-diff.png"));
        }
        QVERIFY2(match,qPrintable(QString("CPU/GPU difference: mean=%1, unmatched=%2, colored unmatched=%3; images: %4")
            .arg(difference.mean).arg(difference.unmatched).arg(difference.colorUnmatched).arg(directory)));
    }
void RendererTest::rejectsMissingGeometryAndChangedColors()
{
    QImage reference(100,100,QImage::Format_RGB32);reference.fill(Qt::white);
    QImage missing=reference,wrong=reference;
    for (int y=20;y<80;++y) for(int x=20;x<80;++x) { reference.setPixel(x,y,qRgb(255,0,0));wrong.setPixel(x,y,qRgb(0,0,255)); }
    const auto blank=compare(reference,missing),changed=compare(reference,wrong),same=compare(reference,reference);
    QVERIFY(blank.mean>0.015);QVERIFY(blank.colorUnmatched>0.025);QVERIFY(blank.coloredMismatchPixels>64);
    QVERIFY(changed.mean>0.015);QVERIFY(changed.colorUnmatched>0.025);QVERIFY(changed.coloredMismatchPixels>64);
    QCOMPARE(same.mean,0.);QCOMPARE(same.unmatched,0.);QCOMPARE(same.colorUnmatched,0.);
    // A missing thin series must fail even when the average image error is low.
    reference.fill(Qt::white); wrong.fill(Qt::white);
    for (int x=10;x<90;++x) reference.setPixel(x,50,qRgb(255,0,0));
    const auto line=compare(reference,wrong);
    QVERIFY(line.mean<0.015); QVERIFY(line.unmatched<0.012);
    QVERIFY(line.colorUnmatched>0.025); QVERIFY(line.coloredMismatchPixels>64);
}
QTEST_MAIN(RendererTest)
#include "test_renderers.moc"
