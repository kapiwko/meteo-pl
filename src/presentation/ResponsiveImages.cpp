#include "ResponsiveImages.hpp"
#include <QAbstractTextDocumentLayout>
#include <QImage>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextImageFormat>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <utility>

void ResponsiveImages::setDocument(QQuickTextDocument *value)
{
    if (m_document == value) return;
    for (const auto &connection : std::as_const(m_connections)) disconnect(connection);
    m_connections.clear();
    m_document = value;
    if (value) {
        auto *doc=value->textDocument();
        m_connections.append(connect(doc,&QTextDocument::contentsChanged,this,&ResponsiveImages::schedule));
        m_connections.append(connect(doc->documentLayout(),&QAbstractTextDocumentLayout::documentSizeChanged,this,&ResponsiveImages::schedule));
    }
    emit documentChanged();
    schedule();
}
void ResponsiveImages::setMaximumWidth(qreal value)
{
    if (qFuzzyCompare(m_width,value)) return;
    m_width=value;
    emit maximumWidthChanged();
    schedule();
}
void ResponsiveImages::schedule()
{
    if (m_pending) return;
    m_pending=true;
    QTimer::singleShot(0,this,[this] { m_pending=false; resizeImages(); });
}
void ResponsiveImages::resizeImages()
{
    if (!m_document || !std::isfinite(m_width) || m_width<=0) return;
    auto *doc=m_document->textDocument();
    constexpr int originalWidth=QTextFormat::UserProperty+1, originalHeight=QTextFormat::UserProperty+2;
    struct Change { int position, length; QTextImageFormat format; };
    QList<Change> changes;
    for (auto block=doc->begin();block.isValid();block=block.next()) {
        for (auto it=block.begin();!it.atEnd();++it) {
            const auto fragment=it.fragment();
            if (!fragment.charFormat().isImageFormat()) continue;
            auto format=fragment.charFormat().toImageFormat();
            const auto resource=doc->resource(QTextDocument::ImageResource,QUrl(format.name()));
            const auto image=qvariant_cast<QImage>(resource);
            qreal width=format.hasProperty(originalWidth)?format.doubleProperty(originalWidth):format.width();
            qreal height=format.hasProperty(originalHeight)?format.doubleProperty(originalHeight):format.height();
            if (width<=0 && !image.isNull()) width=height>0?height*image.width()/image.height():image.width()/image.devicePixelRatio();
            if (height<=0 && width>0 && !image.isNull()) height=width*image.height()/image.width();
            if (width<=0 || height<=0) continue; // Retry when the asynchronous image resource arrives.
            const auto target=std::min(width,m_width), targetHeight=height*target/width;
            if (qAbs(format.width()-target)<0.1 && qAbs(format.height()-targetHeight)<0.1) continue;
            format.setProperty(originalWidth,width);format.setProperty(originalHeight,height);
            format.setWidth(target);format.setHeight(targetHeight);
            changes.append({fragment.position(),fragment.length(),format});
        }
    }
    for (const auto &change:changes) {
        QTextCursor cursor(doc);
        cursor.setPosition(change.position);cursor.setPosition(change.position+change.length,QTextCursor::KeepAnchor);
        cursor.setCharFormat(change.format);
    }
}
