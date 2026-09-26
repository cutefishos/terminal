#include "TerminalGpuNode.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>
#include <QSGTextureMaterial>
#include <QSGVertexColorMaterial>
#include <QtMath>

#include <cstring>

#include "LineChars.h"
#include "konsole_wcwidth.h"

using namespace Konsole;

namespace Konsole
{

// Coverage from the atlas's alpha, tinted by the vertex colour.
class GlyphShader : public QSGMaterialShader
{
public:
    GlyphShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/qmltermwidget/shaders/terminalglyph.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/qmltermwidget/shaders/terminalglyph.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override
    {
        QByteArray *buffer = state.uniformData();
        Q_ASSERT(buffer->size() >= 68);
        bool changed = false;

        if (state.isMatrixDirty()) {
            const QMatrix4x4 matrix = state.combinedMatrix();
            std::memcpy(buffer->data(), matrix.constData(), 64);
            changed = true;
        }
        if (state.isOpacityDirty()) {
            const float opacity = state.opacity();
            std::memcpy(buffer->data() + 64, &opacity, 4);
            changed = true;
        }
        return changed;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                            QSGMaterial *newMaterial, QSGMaterial *) override;
};

class GlyphMaterial : public QSGMaterial
{
public:
    GlyphMaterial() { setFlag(Blending); }

    QSGMaterialType *type() const override
    {
        static QSGMaterialType type;
        return &type;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override
    {
        return new GlyphShader;
    }

    int compare(const QSGMaterial *other) const override
    {
        const auto *material = static_cast<const GlyphMaterial *>(other);
        if (texture == material->texture)
            return 0;
        return texture < material->texture ? -1 : 1;
    }

    QSGTexture *texture = nullptr;
};

void GlyphShader::updateSampledImage(RenderState &state, int binding, QSGTexture **texture,
                                     QSGMaterial *newMaterial, QSGMaterial *)
{
    if (binding != 1)
        return;

    auto *material = static_cast<GlyphMaterial *>(newMaterial);
    if (material->texture)
        material->texture->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
    *texture = material->texture;
}

}

namespace
{

const int AtlasWidth = 1024;
const int AtlasMaxHeight = 2048;

const QSGGeometry::AttributeSet &glyphAttributes()
{
    static const QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2, 4, QSGGeometry::UnsignedByteType, QSGGeometry::ColorAttribute),
    };
    static const QSGGeometry::AttributeSet set = { 3, 20, attributes };
    return set;
}

QSGGeometryNode *createNode(const QSGGeometry::AttributeSet &attributes, QSGMaterial *material)
{
    auto *geometry = new QSGGeometry(attributes, 0, 0, QSGGeometry::UnsignedIntType);
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
    geometry->setIndexDataPattern(QSGGeometry::DynamicPattern);

    auto *node = new QSGGeometryNode;
    node->setGeometry(geometry);
    node->setMaterial(material);
    node->setFlags(QSGNode::OwnsGeometry | QSGNode::OwnsMaterial);
    return node;
}

// Vertices come in quads: top left, top right, bottom left, bottom right.
template<typename Vertex>
void fillGeometry(QSGGeometryNode *node, const std::vector<Vertex> &vertices)
{
    QSGGeometry *geometry = node->geometry();
    const int quads = int(vertices.size() / 4);
    geometry->allocate(int(vertices.size()), quads * 6);
    if (!vertices.empty())
        std::memcpy(geometry->vertexData(), vertices.data(), vertices.size() * sizeof(Vertex));

    quint32 *indices = geometry->indexDataAsUInt();
    for (int quad = 0; quad < quads; ++quad) {
        const quint32 base = quint32(quad) * 4;
        indices[0] = base;
        indices[1] = base + 1;
        indices[2] = base + 2;
        indices[3] = base + 2;
        indices[4] = base + 1;
        indices[5] = base + 3;
        indices += 6;
    }
    node->markDirty(QSGNode::DirtyGeometry);
}

}

TerminalGpuNode::TerminalGpuNode(QQuickWindow *window)
    : m_window(window)
    , m_underMaterial(new QSGVertexColorMaterial)
    , m_glyphMaterial(new GlyphMaterial)
    , m_colorGlyphMaterial(new QSGTextureMaterial)
    , m_overMaterial(new QSGVertexColorMaterial)
{
    m_colorGlyphMaterial->setFlag(QSGMaterial::Blending);
    m_colorGlyphMaterial->setFiltering(QSGTexture::Linear);

    m_underNode = createNode(QSGGeometry::defaultAttributes_ColoredPoint2D(), m_underMaterial);
    m_glyphNode = createNode(glyphAttributes(), m_glyphMaterial);
    m_colorGlyphNode = createNode(QSGGeometry::defaultAttributes_TexturedPoint2D(), m_colorGlyphMaterial);
    m_overNode = createNode(QSGGeometry::defaultAttributes_ColoredPoint2D(), m_overMaterial);

    appendChildNode(m_underNode);
    appendChildNode(m_glyphNode);
    appendChildNode(m_colorGlyphNode);
    appendChildNode(m_overNode);
}

TerminalGpuNode::~TerminalGpuNode()
{
    delete m_texture;
}

void TerminalGpuNode::update(const TerminalFrame &frame)
{
    m_dpr = frame.devicePixelRatio;
    m_origin = QPointF(frame.deviceOrigin.x() - std::floor(frame.deviceOrigin.x()),
                       frame.deviceOrigin.y() - std::floor(frame.deviceOrigin.y()));

    if (m_atlas.isNull() || m_atlasFont != frame.font || m_atlasDpr != m_dpr
            || m_atlasAscent != frame.fontAscent || m_atlasLineSpacing != frame.lineSpacing
            || m_atlasFontWidth != frame.fontWidth || m_atlasFontHeight != frame.fontHeight)
        resetAtlas(frame);

    // A frame that fills the atlas starts it over once; what still does not fit is left out.
    if (!build(frame)) {
        resetAtlas(frame);
        build(frame);
    }

    if (m_atlasDirty) {
        delete m_texture;
        m_texture = m_window->createTextureFromImage(m_atlas);
        m_texture->setFiltering(QSGTexture::Linear);
        m_glyphMaterial->texture = m_texture;
        m_colorGlyphMaterial->setTexture(m_texture);
        m_glyphNode->markDirty(QSGNode::DirtyMaterial);
        m_colorGlyphNode->markDirty(QSGNode::DirtyMaterial);
        m_atlasDirty = false;
    }

    uploadGeometry();
}

void TerminalGpuNode::resetAtlas(const TerminalFrame &frame)
{
    m_atlasFont = frame.font;
    m_atlasDpr = m_dpr;
    m_atlasAscent = frame.fontAscent;
    m_atlasLineSpacing = frame.lineSpacing;
    m_atlasFontWidth = frame.fontWidth;
    m_atlasFontHeight = frame.fontHeight;

    m_atlas = QImage(AtlasWidth, 256, QImage::Format_ARGB32_Premultiplied);
    m_atlas.fill(Qt::transparent);
    m_atlasDirty = true;
    m_atlasFull = false;
    m_shelfX = 0;
    m_shelfY = 0;
    m_shelfHeight = 0;
    m_glyphTable.clear();
    m_singleGlyphs.clear();
    m_clusterGlyphs.clear();
}

const TerminalGpuNode::Glyph *TerminalGpuNode::singleGlyph(char32_t codepoint, quint8 style, int cells)
{
    const quint64 key = quint64(codepoint) | (quint64(style) << 32) | (quint64(cells) << 40);
    const auto found = m_singleGlyphs.constFind(key);
    if (found != m_singleGlyphs.constEnd())
        return &m_glyphTable[*found];

    const Glyph *glyph = rasterise(QString::fromUcs4(&codepoint, 1), style, cells);
    if (glyph)
        m_singleGlyphs.insert(key, int(m_glyphTable.size()) - 1);
    return glyph;
}

const TerminalGpuNode::Glyph *TerminalGpuNode::clusterGlyph(const QString &text, quint8 style, int cells)
{
    const QString key = QChar(ushort(style | (cells << 4))) + text;
    const auto found = m_clusterGlyphs.constFind(key);
    if (found != m_clusterGlyphs.constEnd())
        return &m_glyphTable[*found];

    const Glyph *glyph = rasterise(text, style, cells);
    if (glyph)
        m_clusterGlyphs.insert(key, int(m_glyphTable.size()) - 1);
    return glyph;
}

// Paints like TerminalDisplay::drawCharacters(), white on a padded scratch
// image, and keeps the inked part. The cell's corner sits on a whole device pixel.
const TerminalGpuNode::Glyph *TerminalGpuNode::rasterise(const QString &text, quint8 style, int cells)
{
    const int padX = qCeil(m_atlasFontWidth * m_dpr);
    const int padY = qCeil(m_atlasFontHeight * m_dpr / 2);
    QImage scratch(qCeil(cells * m_atlasFontWidth * m_dpr) + 2 * padX,
                   qCeil(m_atlasFontHeight * m_dpr) + 2 * padY,
                   QImage::Format_ARGB32_Premultiplied);
    scratch.fill(Qt::transparent);

    {
        QPainter painter(&scratch);
        painter.translate(padX, padY);
        painter.scale(m_dpr, m_dpr);

        if (style & BoxDrawing) {
            QPen pen(Qt::white);
            if (style & Bold)
                pen.setWidth(3);
            painter.setPen(pen);
            drawOtherLineChar(painter, 0, 0, m_atlasFontWidth, m_atlasFontHeight, uchar(text.at(0).unicode() & 0xff));
        } else {
            QFont font = m_atlasFont;
            font.setBold(style & Bold);
            font.setItalic(style & Italic);
            painter.setFont(font);
            painter.setPen(Qt::white);
            painter.setLayoutDirection(Qt::LeftToRight);
            painter.drawText(QPointF(0, m_atlasAscent + m_atlasLineSpacing), text);
        }
    }

    int left = scratch.width();
    int right = -1;
    int top = scratch.height();
    int bottom = -1;
    bool color = false;
    for (int y = 0; y < scratch.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(scratch.constScanLine(y));
        for (int x = 0; x < scratch.width(); ++x) {
            const QRgb pixel = line[x];
            if (!qAlpha(pixel))
                continue;
            left = qMin(left, x);
            right = qMax(right, x);
            top = qMin(top, y);
            bottom = qMax(bottom, y);
            if (qRed(pixel) != qGreen(pixel) || qGreen(pixel) != qBlue(pixel))
                color = true;
        }
    }

    Glyph glyph;
    if (right >= 0) {
        const QRect ink(left, top, right - left + 1, bottom - top + 1);
        // A one pixel gutter keeps neighbours out of a stretched double-width glyph.
        const int width = ink.width() + 1;
        const int height = ink.height() + 1;
        if (width > m_atlas.width())
            return nullptr;

        if (m_shelfX + width > m_atlas.width()) {
            m_shelfY += m_shelfHeight;
            m_shelfX = 0;
            m_shelfHeight = 0;
        }
        if (m_shelfY + height > m_atlas.height()) {
            int newHeight = m_atlas.height();
            while (newHeight < m_shelfY + height && newHeight < AtlasMaxHeight)
                newHeight *= 2;
            if (m_shelfY + height > newHeight) {
                m_atlasFull = true;
                return nullptr;
            }
            QImage grown(m_atlas.width(), newHeight, QImage::Format_ARGB32_Premultiplied);
            grown.fill(Qt::transparent);
            std::memcpy(grown.bits(), m_atlas.constBits(), m_atlas.sizeInBytes());
            m_atlas = grown;
        }

        for (int y = 0; y < ink.height(); ++y) {
            std::memcpy(m_atlas.scanLine(m_shelfY + y) + m_shelfX * 4,
                        scratch.constScanLine(ink.top() + y) + ink.left() * 4,
                        ink.width() * 4);
        }

        glyph.atlasRect = QRect(m_shelfX, m_shelfY, ink.width(), ink.height());
        glyph.offset = ink.topLeft() - QPoint(padX, padY);
        glyph.color = color;
        m_shelfX += width;
        m_shelfHeight = qMax(m_shelfHeight, height);
        m_atlasDirty = true;
    }

    m_glyphTable.push_back(glyph);
    return &m_glyphTable.back();
}

int TerminalGpuNode::deviceX(qreal logical) const
{
    return qRound(m_origin.x() + logical * m_dpr);
}

int TerminalGpuNode::deviceY(qreal logical) const
{
    return qRound(m_origin.y() + logical * m_dpr);
}

QPointF TerminalGpuNode::toLocal(qreal x, qreal y) const
{
    return QPointF((x - m_origin.x()) / m_dpr, (y - m_origin.y()) / m_dpr);
}

void TerminalGpuNode::addSolid(std::vector<QSGGeometry::ColoredPoint2D> &vertices, const QRect &device,
                               const QColor &color)
{
    if (device.isEmpty() || !color.alpha())
        return;

    const int a = color.alpha();
    const uchar r = uchar(color.red() * a / 255);
    const uchar g = uchar(color.green() * a / 255);
    const uchar b = uchar(color.blue() * a / 255);
    const QPointF topLeft = toLocal(device.left(), device.top());
    const QPointF bottomRight = toLocal(device.left() + device.width(), device.top() + device.height());

    QSGGeometry::ColoredPoint2D vertex;
    vertex.set(float(topLeft.x()), float(topLeft.y()), r, g, b, uchar(a));
    vertices.push_back(vertex);
    vertex.set(float(bottomRight.x()), float(topLeft.y()), r, g, b, uchar(a));
    vertices.push_back(vertex);
    vertex.set(float(topLeft.x()), float(bottomRight.y()), r, g, b, uchar(a));
    vertices.push_back(vertex);
    vertex.set(float(bottomRight.x()), float(bottomRight.y()), r, g, b, uchar(a));
    vertices.push_back(vertex);
}

// Texture coordinates stay in atlas pixels until upload: the atlas may grow later in the frame.
void TerminalGpuNode::addGlyph(const Glyph &glyph, int x, int y, int scaleX, int scaleY, const QColor &color)
{
    if (glyph.atlasRect.isEmpty())
        return;

    const QRect &source = glyph.atlasRect;
    const int left = x + glyph.offset.x() * scaleX;
    const int top = y + glyph.offset.y() * scaleY;
    const QPointF topLeft = toLocal(left, top);
    const QPointF bottomRight = toLocal(left + source.width() * scaleX, top + source.height() * scaleY);
    const float u0 = float(source.left());
    const float v0 = float(source.top());
    const float u1 = float(source.left() + source.width());
    const float v1 = float(source.top() + source.height());

    if (glyph.color) {
        QSGGeometry::TexturedPoint2D vertex;
        vertex.set(float(topLeft.x()), float(topLeft.y()), u0, v0);
        m_colorGlyphs.push_back(vertex);
        vertex.set(float(bottomRight.x()), float(topLeft.y()), u1, v0);
        m_colorGlyphs.push_back(vertex);
        vertex.set(float(topLeft.x()), float(bottomRight.y()), u0, v1);
        m_colorGlyphs.push_back(vertex);
        vertex.set(float(bottomRight.x()), float(bottomRight.y()), u1, v1);
        m_colorGlyphs.push_back(vertex);
        return;
    }

    const int a = color.alpha();
    const uchar r = uchar(color.red() * a / 255);
    const uchar g = uchar(color.green() * a / 255);
    const uchar b = uchar(color.blue() * a / 255);
    m_glyphs.push_back({ float(topLeft.x()), float(topLeft.y()), u0, v0, r, g, b, uchar(a) });
    m_glyphs.push_back({ float(bottomRight.x()), float(topLeft.y()), u1, v0, r, g, b, uchar(a) });
    m_glyphs.push_back({ float(topLeft.x()), float(bottomRight.y()), u0, v1, r, g, b, uchar(a) });
    m_glyphs.push_back({ float(bottomRight.x()), float(bottomRight.y()), u1, v1, r, g, b, uchar(a) });
}

// drawLineChar() as device pixel rects. Edges go through the same mapping as
// the cells, so strokes cover what QPainter's aliased lines would, and meet
// the neighbouring cells' strokes exactly.
void TerminalGpuNode::addLineChar(qreal left, qreal top, int scaleX, int scaleY, uint8_t code, bool bold,
                                  const QColor &color)
{
    const quint32 toDraw = LineChars[code];
    const int w = m_atlasFontWidth;
    const int h = m_atlasFontHeight;
    const int cx = w / 2;
    const int cy = h / 2;
    // A bold stroke is three pixels wide, and its square caps reach one further.
    const int grow = bold ? 1 : 0;

    auto rect = [&](int x0, int y0, int x1, int y1) {
        const int left0 = deviceX(left + qBound(0, x0, w) * scaleX);
        const int top0 = deviceY(top + qBound(0, y0, h) * scaleY);
        const int right = qMax(left0 + 1, deviceX(left + qBound(0, x1, w) * scaleX));
        const int bottom = qMax(top0 + 1, deviceY(top + qBound(0, y1, h) * scaleY));
        addSolid(m_over, QRect(left0, top0, right - left0, bottom - top0), color);
    };
    // Both ends inclusive, as QPainter::drawLine() takes them.
    auto vertical = [&](int x, int y0, int y1) { rect(x - grow, y0 - grow, x + 1 + grow, y1 + 1 + grow); };
    auto horizontal = [&](int y, int x0, int x1) { rect(x0 - grow, y - grow, x1 + 1 + grow, y + 1 + grow); };
    auto point = [&](int x, int y) { rect(x - grow, y - grow, x + 1 + grow, y + 1 + grow); };

    if (toDraw & TopL)
        vertical(cx - 1, 0, cy - 2);
    if (toDraw & TopC)
        vertical(cx, 0, cy - 2);
    if (toDraw & TopR)
        vertical(cx + 1, 0, cy - 2);

    if (toDraw & BotL)
        vertical(cx - 1, cy + 2, h - 1);
    if (toDraw & BotC)
        vertical(cx, cy + 2, h - 1);
    if (toDraw & BotR)
        vertical(cx + 1, cy + 2, h - 1);

    if (toDraw & LeftT)
        horizontal(cy - 1, 0, cx - 2);
    if (toDraw & LeftC)
        horizontal(cy, 0, cx - 2);
    if (toDraw & LeftB)
        horizontal(cy + 1, 0, cx - 2);

    if (toDraw & RightT)
        horizontal(cy - 1, cx + 2, w - 1);
    if (toDraw & RightC)
        horizontal(cy, cx + 2, w - 1);
    if (toDraw & RightB)
        horizontal(cy + 1, cx + 2, w - 1);

    if (toDraw & Int11)
        point(cx - 1, cy - 1);
    if (toDraw & Int12)
        point(cx, cy - 1);
    if (toDraw & Int13)
        point(cx + 1, cy - 1);

    if (toDraw & Int21)
        point(cx - 1, cy);
    if (toDraw & Int22)
        point(cx, cy);
    if (toDraw & Int23)
        point(cx + 1, cy);

    if (toDraw & Int31)
        point(cx - 1, cy + 1);
    if (toDraw & Int32)
        point(cx, cy + 1);
    if (toDraw & Int33)
        point(cx + 1, cy + 1);
}

// Mirrors TerminalDisplay::paint(). The display's filter chain holds no
// filters (QML underlines links), so paintFilters() has nothing to add.
bool TerminalGpuNode::build(const TerminalFrame &frame)
{
    m_under.clear();
    m_glyphs.clear();
    m_colorGlyphs.clear();
    m_over.clear();

    const int fw = frame.fontWidth;
    const int fh = frame.fontHeight;
    const int lineThickness = qMax(1, qRound(m_dpr));
    const QFontMetricsF metrics(frame.font);
    const qreal underlinePos = metrics.underlinePos();
    const qreal strikeOutPos = metrics.strikeOutPos();
    const qreal overlinePos = metrics.overlinePos();
    const int decorationThickness = qMax(1, qRound(metrics.lineWidth() * m_dpr));

    if (frame.backgroundOpacity > 0) {
        QColor background = frame.background;
        background.setAlphaF(frame.backgroundOpacity);
        addSolid(m_under, QRect(QPoint(deviceX(0), deviceY(0)),
                                QPoint(deviceX(frame.size.width()) - 1, deviceY(frame.size.height()) - 1)),
                 background);
    }

    // Returns whether the text under the cursor takes the cell's background colour.
    auto addCursor = [&](qreal left, qreal top, qreal width, qreal scaleY, const QColor &foreground) {
        const QColor color = frame.cursorColor.isValid() ? frame.cursorColor : foreground;
        const qreal height = (fh - (frame.fullCursorHeight ? 0 : int(frame.lineSpacing) - 1)) * scaleY;
        const QRect rect(QPoint(deviceX(left), deviceY(top)),
                         QPoint(deviceX(left + width) - 1, deviceY(top + height) - 1));

        switch (frame.cursorShape) {
        case 1:
            addSolid(m_over, QRect(rect.left(), rect.bottom() + 1 - lineThickness, rect.width(), lineThickness), color);
            return false;
        case 2:
            addSolid(m_over, QRect(rect.left(), rect.top(), lineThickness, rect.height()), color);
            return false;
        default:
            addSolid(m_under, rect, color);
            return !frame.cursorColor.isValid();
        }
    };

    auto addDecorations = [&](const QRect &cell, qreal top, int scaleY, bool underline, const QColor &color) {
        const qreal baseline = top + (frame.fontAscent + frame.lineSpacing) * scaleY;
        auto line = [&](qreal y) {
            addSolid(m_over, QRect(cell.left(), deviceY(y), cell.width(), decorationThickness * scaleY), color);
        };
        if (underline)
            line(baseline + underlinePos * scaleY);
        if (frame.font.strikeOut())
            line(baseline - strikeOutPos * scaleY);
        if (frame.font.overline())
            line(baseline - overlinePos * scaleY);
    };

    auto styleOf = [&](const Character &c) {
        quint8 style = 0;
        if (((c.rendition & RE_BOLD) && frame.boldIntense) || frame.font.bold())
            style |= Bold;
        if (((c.rendition & RE_ITALIC) && frame.italicEnabled) || frame.font.italic())
            style |= Italic;
        return style;
    };

    const int preeditCells = frame.preedit.empty() ? 0 : string_width(frame.preedit);

    for (int y = 0; y < frame.usedLines; ++y) {
        const LineProperty property = y < frame.lineProperties->size() ? frame.lineProperties->at(y) : LineProperty(0);
        const int scaleX = (property & LINE_DOUBLEWIDTH) ? 2 : 1;
        const int scaleY = (property & LINE_DOUBLEHEIGHT) ? 2 : 1;
        const qreal top = frame.topMargin + y * fh;
        const int rowTop = deviceY(top);
        const int rowBottom = deviceY(top + fh * scaleY);
        const Character *line = frame.image + y * frame.columns;

        for (int x = 0; x < frame.usedColumns; ++x) {
            const Character &c = line[x];
            const int cells = (x + 1 < frame.usedColumns && line[x + 1].character == 0) ? 2 : 1;

            // The preedit string covers these cells.
            if (preeditCells && y == frame.cursorPosition.y() && x >= frame.cursorPosition.x()
                    && x < frame.cursorPosition.x() + preeditCells) {
                x += cells - 1;
                continue;
            }

            const qreal left = frame.leftMargin + x * fw * scaleX;
            const qreal width = fw * scaleX * cells;
            const QRect cell(QPoint(deviceX(left), rowTop), QPoint(deviceX(left + width) - 1, rowBottom - 1));
            const QColor foreground = c.foregroundColor.color(frame.colorTable);
            const QColor background = c.backgroundColor.color(frame.colorTable);

            if (background != frame.background)
                addSolid(m_under, cell, background);

            bool invert = false;
            if ((c.rendition & RE_CURSOR) && !frame.cursorBlinking)
                invert = addCursor(left, top, width, scaleY, foreground);

            x += cells - 1;

            if (frame.blinking && (c.rendition & RE_BLINK))
                continue;

            const QColor color = invert ? background : foreground;
            const quint8 style = styleOf(c);
            addDecorations(cell, top, scaleY, c.rendition & RE_UNDERLINE || frame.font.underline(), color);

            if (!c.character || c.character == L' ')
                continue;

            if (frame.drawLineChars && c.isLineChar()) {
                const uint8_t code = uint8_t(c.character & 0xff);
                const bool bold = (c.rendition & RE_BOLD) && frame.boldIntense;
                if (LineChars[code]) {
                    addLineChar(left, top, scaleX, scaleY, code, bold, color);
                } else if (const Glyph *glyph = singleGlyph(char32_t(c.character), BoxDrawing | (bold ? Bold : 0), 1)) {
                    addGlyph(*glyph, cell.left(), rowTop, scaleX, scaleY, color);
                }
                continue;
            }

            const Glyph *glyph = nullptr;
            if (c.rendition & RE_EXTENDED_CHAR) {
                ushort length = 0;
                const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(c.character, length);
                if (chars)
                    glyph = clusterGlyph(QString::fromUcs4(reinterpret_cast<const char32_t *>(chars), length), style, cells);
            } else {
                glyph = singleGlyph(char32_t(c.character), style, cells);
            }
            if (glyph)
                addGlyph(*glyph, cell.left(), rowTop, scaleX, scaleY, color);
        }

        // Both halves of a double-height line hold the same text; the first draws it.
        if (scaleY == 2)
            ++y;
    }

    // drawInputMethodPreeditString(): the default background, the cursor, then the text.
    if (preeditCells && frame.cursorPosition.y() < frame.usedLines) {
        const qreal left = frame.leftMargin + frame.cursorPosition.x() * fw;
        const qreal top = frame.topMargin + frame.cursorPosition.y() * fh;
        const int rowTop = deviceY(top);
        const Character &style = frame.image[frame.cursorPosition.y() * frame.columns + frame.cursorPosition.x()];

        QColor background = frame.colorTable[DEFAULT_BACK_COLOR].color;
        background.setAlphaF(frame.backgroundOpacity);
        addSolid(m_under, QRect(QPoint(deviceX(left), rowTop),
                                QPoint(deviceX(left + fw * preeditCells) - 1, deviceY(top + fh) - 1)),
                 background);

        bool invert = false;
        if (!frame.cursorBlinking)
            invert = addCursor(left, top, fw * preeditCells, 1, frame.colorTable[DEFAULT_FORE_COLOR].color);

        const QColor color = (invert ? style.backgroundColor : style.foregroundColor).color(frame.colorTable);
        const quint8 glyphStyle = styleOf(style);
        int column = 0;
        size_t i = 0;
        while (i < frame.preedit.size()) {
            QString cluster = QString::fromUcs4(reinterpret_cast<const char32_t *>(&frame.preedit[i]), 1);
            const int cells = qMax(1, konsole_wcwidth(frame.preedit[i]));
            for (++i; i < frame.preedit.size() && konsole_wcwidth(frame.preedit[i]) == 0; ++i)
                cluster += QString::fromUcs4(reinterpret_cast<const char32_t *>(&frame.preedit[i]), 1);

            if (const Glyph *glyph = clusterGlyph(cluster, glyphStyle, cells))
                addGlyph(*glyph, deviceX(left + column * fw), rowTop, 1, 1, color);
            column += cells;
        }
    }

    return !m_atlasFull;
}

void TerminalGpuNode::uploadGeometry()
{
    const float width = float(m_atlas.width());
    const float height = float(m_atlas.height());
    for (GlyphVertex &vertex : m_glyphs) {
        vertex.u /= width;
        vertex.v /= height;
    }
    for (QSGGeometry::TexturedPoint2D &vertex : m_colorGlyphs) {
        vertex.tx /= width;
        vertex.ty /= height;
    }

    fillGeometry(m_underNode, m_under);
    fillGeometry(m_glyphNode, m_glyphs);
    fillGeometry(m_colorGlyphNode, m_colorGlyphs);
    fillGeometry(m_overNode, m_over);
}
