#include "ThematicMapSource.hpp"
#include "core/Localization.hpp"
#include <QDateTime>
#include <QTimeZone>
#include <algorithm>

namespace Meteo
{
ThematicMapSource::ThematicMapSource(QUrl endpoint) : m_endpoint(std::move(endpoint)) {}
const QVector<ThematicLayer> &ThematicMapSource::layers()
{
    static const QVector<ThematicLayer> catalog{
        {Meteo::i18n("UM4 · pressure"), "SLPH_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · temperature and streamlines"), "T+WH_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · surface temperature"), "GT_H_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · precipitation"), "RS_H_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · storms"), "FLSH_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · surface wind"), "W__H_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · maximum gusts"), "WGSH_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · visibility"), "VISH_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · fog"), "F__H_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · humidity over ice"), "RHiH_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · humidity over water"), "RHwH_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · very low clouds"), "CV_H_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · low clouds"), "CL_H_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · medium clouds"), "CM_H_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · high clouds"), "CH_H_0000.0_0U", "um"},
        {Meteo::i18n("UM4 · total cloud cover"), "CT_H_0000.0_0U", "um"},
        {Meteo::i18n("WAM · significant wave height"), "wavehgt_0W", "wamcoamps"},
        {Meteo::i18n("WAM · mean wave period"), "m_period_0W", "wamcoamps"},
        {Meteo::i18n("WAM · peak wave period"), "p_period_0W", "wamcoamps"}};
    return catalog;
}
const ThematicLayer &ThematicMapSource::layer(int index)
{
    return layers().at(std::clamp(index, 0, int(layers().size()) - 1));
}
int ThematicMapSource::boundedStep(int index, int hour)
{
    const auto &selected = layer(index);
    return std::clamp(hour, selected.stepHours(), selected.lastStep()) / selected.stepHours() * selected.stepHours();
}
qint64 ThematicMapSource::latestRun(int index, qint64 now)
{
    if (now <= 0 || now > 253402300799LL)
        return 0;
    const qint64 period = layer(index).runPeriodHours() * 3600;
    return now / period * period;
}
QUrl ThematicMapSource::imageUrl(int index, qint64 run, int hour) const
{
    if (run <= 0 || run > 253402300799LL)
        return {};
    const auto &selected = layer(index);
    const auto stamp = QDateTime::fromSecsSinceEpoch(run, QTimeZone::UTC).toString(QStringLiteral("yyyyMMddhh"));
    const auto path = '/' + selected.model.toUtf8() + "/pict/" + stamp.toUtf8() + '/' +
                      QUrl::toPercentEncoding(selected.id) + '_' + stamp.toUtf8() + '_' +
                      QByteArray::number(boundedStep(index, hour)).rightJustified(3, '0') + "-00.png";
    return m_endpoint.resolved(QUrl::fromEncoded(path));
}
} // namespace Meteo
