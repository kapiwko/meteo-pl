#include "core/Localization.hpp"
#include "ChartExporter.hpp"
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImageWriter>
#include <QJSValue>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr int margin = 16, gap = 22;
constexpr qint64 maximumPixels = 64 * 1024 * 1024;
}

ChartExporter::ChartExporter(QObject *parent) : QObject(parent)
{
    m_deadline.setSingleShot(true);
    m_deadline.setInterval(15000);
    connect(&m_deadline, &QTimer::timeout, this, [this] { fail(Meteo::i18n("Image preparation timed out. Try again.")); });
}
ChartExporter::~ChartExporter() { restore(); }

QUrl ChartExporter::suggestedFile(const QString &location, const QString &chart, const QString &date) const
{
    auto name = QStringLiteral("%1-%2-%3").arg(location, chart.isEmpty() ? QStringLiteral("meteogram") : chart, date);
    name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|\x00-\x1f])")), QStringLiteral("_"));
    const auto folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return QUrl::fromLocalFile(QDir(folder.isEmpty() ? QDir::homePath() : folder).filePath(name.left(180) + QStringLiteral(".png")));
}

bool ChartExporter::savePng(const QUrl &file, const QVariantList &items, const QVariantMap &metadata)
{
    if (m_busy) return false;
    m_error.clear();
    if (!file.isLocalFile() || file.toLocalFile().isEmpty()) { fail(Meteo::i18n("Choose a local PNG file.")); return false; }
    if (items.isEmpty() || items.size() > 50) { fail(Meteo::i18n("Select between 1 and 50 charts to export.")); return false; }
    int contentWidth = 0;
    qint64 contentHeight = 0;
    for (const auto &value : items) {
        auto *object = value.value<QObject *>();
        if (!object && value.metaType() == QMetaType::fromType<QJSValue>()) object = value.value<QJSValue>().toQObject();
        auto *item = qobject_cast<QQuickItem *>(object);
        if (!item || !item->window() || !item->isVisible() || !std::isfinite(item->width()) || !std::isfinite(item->height())
            || item->width() < 1 || item->height() < 1 || item->width() > 8192 || item->height() > 8192) {
            fail(Meteo::i18n("The chart is not ready to export. Open the meteogram and try again.")); return false;
        }
        const QSize size(qCeil(item->width()), qCeil(item->height()));
        m_items.append({item, size, item->property("exportMode")});
        contentWidth = std::max(contentWidth, size.width());
        contentHeight += size.height();
    }
    const auto title = metadata.value("title").toString().left(4096);
    const auto details = metadata.value("details").toString().left(4096);
    const auto source = metadata.value("source", QStringLiteral("ICM UW / meteo.pl · https://www.meteo.pl/")).toString().left(4096);
    auto titleFont = QGuiApplication::font(), textFont = titleFont;
    titleFont.setPixelSize(18); titleFont.setBold(true); textFont.setPixelSize(11);
    const auto textBounds = [contentWidth](const QFont &font, const QString &text) {
        return QFontMetrics(font).boundingRect(QRect(0, 0, contentWidth, 100000), Qt::TextWordWrap, text).height();
    };
    const int titleHeight = textBounds(titleFont, title), detailsHeight = textBounds(textFont, details), footerHeight = textBounds(textFont, source);
    m_y = margin + titleHeight + 6 + detailsHeight + gap;
    const qint64 height = m_y + contentHeight + (items.size() - 1) * gap + gap + footerHeight + margin;
    const int width = contentWidth + 2 * margin;
    if (height > 65535 || width * height > maximumPixels) {
        fail(Meteo::i18n("The image is too large. Reduce chart heights or export charts separately.")); return false;
    }
    m_image = QImage(width, int(height), QImage::Format_ARGB32_Premultiplied);
    if (m_image.isNull()) { fail(Meteo::i18n("Not enough memory for the meteogram image.")); return false; }
    const auto background = metadata.value("background", QColor(Qt::white)).value<QColor>();
    const auto foreground = metadata.value("foreground", QColor(Qt::black)).value<QColor>();
    m_image.fill(background.isValid() ? background : QColor(Qt::white));
    m_image.setText(QStringLiteral("Title"), title);
    m_image.setText(QStringLiteral("Description"), details);
    m_image.setText(QStringLiteral("Source"), source);
    m_image.setText(QStringLiteral("Software"), QStringLiteral("Meteo Desktop"));
    {
        QPainter painter(&m_image);
        painter.setPen(foreground.isValid() ? foreground : QColor(Qt::black));
        painter.setFont(titleFont);
        painter.drawText(QRect(margin, margin, contentWidth, titleHeight), Qt::TextWordWrap, title);
        painter.setFont(textFont);
        painter.drawText(QRect(margin, margin + titleHeight + 6, contentWidth, detailsHeight), Qt::TextWordWrap, details);
        painter.drawText(QRect(margin, int(height) - margin - footerHeight, contentWidth, footerHeight), Qt::TextWordWrap, source);
    }
    auto path = file.toLocalFile();
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".png");
    m_file = QUrl::fromLocalFile(path);
    m_index = 0; m_busy = true;
    const auto generation = ++m_generation;
    for (const auto &capture : std::as_const(m_items)) {
        if (capture.exportMode.isValid()) capture.item->setProperty("exportMode", true);
        const auto changed = [this, generation] {
            if (m_busy && generation == m_generation) fail(Meteo::i18n("The view changed during export. Try again."));
        };
        m_connections.append(connect(capture.item, &QObject::destroyed, this, changed));
        m_connections.append(connect(capture.item, &QQuickItem::widthChanged, this, changed));
        m_connections.append(connect(capture.item, &QQuickItem::heightChanged, this, changed));
    }
    m_deadline.start();
    emit stateChanged();
    QTimer::singleShot(0, this, [this, generation] { if (generation == m_generation) captureNext(); });
    return true;
}

void ChartExporter::captureNext()
{
    if (!m_busy) return;
    if (m_index == m_items.size()) {
        QSaveFile file(m_file.toLocalFile());
        if (!file.open(QIODevice::WriteOnly)) { fail(Meteo::i18n("Could not save PNG: ") + file.errorString()); return; }
        QImageWriter writer(&file, "png");
        writer.setCompression(3);
        if (!writer.write(m_image)) { fail(Meteo::i18n("Could not create PNG: ") + writer.errorString()); return; }
        if (!file.commit()) { fail(Meteo::i18n("Could not save PNG: ") + file.errorString()); return; }
        const auto savedFile = m_file;
        restore(); m_busy = false;
        emit stateChanged(); emit saved(savedFile, savedFile.toLocalFile());
        return;
    }
    const auto &capture = m_items[m_index];
    if (!capture.item) { fail(Meteo::i18n("The chart was closed during export.")); return; }
    capture.item->ensurePolished();
    m_grab = capture.item->grabToImage(capture.size);
    if (!m_grab) { fail(Meteo::i18n("Could not prepare the chart image.")); return; }
    const auto generation = m_generation;
    connect(m_grab.data(), &QQuickItemGrabResult::ready, this, [this, generation] {
        if (!m_busy || generation != m_generation) return;
        const auto grab = m_grab;
        const auto image = grab->image();
        if (image.isNull()) { fail(Meteo::i18n("The renderer did not return a chart image.")); return; }
        const auto size = m_items[m_index].size;
        {
            QPainter painter(&m_image);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.drawImage(QRect(QPoint(margin, m_y), size), image);
        }
        m_y += size.height() + gap; ++m_index; m_grab.reset();
        QTimer::singleShot(0, this, [this, generation] { if (generation == m_generation) captureNext(); });
    });
}

void ChartExporter::restore()
{
    ++m_generation; m_deadline.stop();
    for (const auto &connection : std::as_const(m_connections)) disconnect(connection);
    m_connections.clear(); m_grab.reset();
    for (const auto &capture : std::as_const(m_items))
        if (capture.item && capture.exportMode.isValid()) capture.item->setProperty("exportMode", capture.exportMode);
    m_items.clear(); m_image = {};
}
void ChartExporter::fail(const QString &message)
{
    restore(); m_busy = false; m_error = message;
    emit stateChanged(); emit failed(message);
}
void ChartExporter::cancel() { if (m_busy) fail(Meteo::i18n("Export cancelled.")); }
