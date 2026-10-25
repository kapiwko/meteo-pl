#pragma once
#include <QUrl>
#include <QVector>

namespace Meteo
{
struct ThematicLayer {
    QString name, id, model;
    bool wave() const { return model != "um"; }
    int stepHours() const { return wave() ? 3 : 1; }
    int lastStep() const { return wave() ? 72 : 60; }
    int runPeriodHours() const { return wave() ? 12 : 6; }
};
class ThematicMapSource
{
public:
    explicit ThematicMapSource(QUrl endpoint = QUrl(QStringLiteral("https://old.meteo.pl")));
    QUrl endpoint() const { return m_endpoint; }
    static const QVector<ThematicLayer> &layers();
    static const ThematicLayer &layer(int index);
    static int boundedStep(int index, int hour);
    static qint64 latestRun(int index, qint64 now);
    QUrl imageUrl(int index, qint64 run, int hour) const;

private:
    QUrl m_endpoint;
};
} // namespace Meteo
