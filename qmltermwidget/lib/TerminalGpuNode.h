#ifndef TERMINALGPUNODE_H
#define TERMINALGPUNODE_H

#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QRect>
#include <QSizeF>
#include <QSGGeometry>
#include <QSGNode>
#include <QVector>

#include <string>
#include <vector>

#include "Character.h"
#include "CharacterColor.h"

class QQuickWindow;
class QSGGeometryNode;
class QSGTexture;
class QSGTextureMaterial;
class QSGVertexColorMaterial;

namespace Konsole
{

class GlyphMaterial;

// What TerminalDisplay::paint() would draw, read while the GUI thread is blocked in sync.
struct TerminalFrame
{
    const Character *image = nullptr;
    int columns = 0;
    int usedLines = 0;
    int usedColumns = 0;
    const QVector<LineProperty> *lineProperties = nullptr;
    const ColorEntry *colorTable = nullptr;

    QFont font;
    int fontWidth = 1;
    int fontHeight = 1;
    int fontAscent = 1;
    int lineSpacing = 0;
    int leftMargin = 0;
    int topMargin = 0;

    bool boldIntense = true;
    bool italicEnabled = true;
    bool drawLineChars = true;
    bool blinking = false;
    bool cursorBlinking = false;
    bool fullCursorHeight = false;
    int cursorShape = 0;
    QColor cursorColor;

    // Cells whose background equals this one are left to the default fill.
    QColor background;
    qreal backgroundOpacity = 1;

    std::wstring preedit;
    QPoint cursorPosition;

    QSizeF size;

    qreal devicePixelRatio = 1;
    // The item's origin on the window's device grid, in device pixels.
    QPointF deviceOrigin;
};

// Draws the terminal with the scene graph: solid quads for backgrounds, the
// cursor and box drawing, and glyphs rasterised once into an atlas texture.
class TerminalGpuNode : public QSGNode
{
public:
    explicit TerminalGpuNode(QQuickWindow *window);
    ~TerminalGpuNode() override;

    void update(const TerminalFrame &frame);

private:
    struct Glyph
    {
        QRect atlasRect;
        // From the cell's top left corner, in device pixels.
        QPoint offset;
        bool color = false;
    };

    struct GlyphVertex
    {
        float x;
        float y;
        float u;
        float v;
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    };

    enum GlyphStyle : quint8 {
        Bold = 1,
        Italic = 2,
        BoxDrawing = 4,
    };

    void resetAtlas(const TerminalFrame &frame);
    // Valid until the next lookup.
    const Glyph *singleGlyph(char32_t codepoint, quint8 style, int cells);
    const Glyph *clusterGlyph(const QString &text, quint8 style, int cells);
    const Glyph *rasterise(const QString &text, quint8 style, int cells);
    // False when the atlas filled up.
    bool build(const TerminalFrame &frame);
    void uploadGeometry();

    int deviceX(qreal logical) const;
    int deviceY(qreal logical) const;
    QPointF toLocal(qreal x, qreal y) const;

    void addSolid(std::vector<QSGGeometry::ColoredPoint2D> &vertices, const QRect &device, const QColor &color);
    void addGlyph(const Glyph &glyph, int x, int y, int scaleX, int scaleY, const QColor &color);
    void addLineChar(qreal left, qreal top, int scaleX, int scaleY, uint8_t code, bool bold, const QColor &color);

    QQuickWindow *m_window;

    QSGGeometryNode *m_underNode;
    QSGGeometryNode *m_glyphNode;
    QSGGeometryNode *m_colorGlyphNode;
    QSGGeometryNode *m_overNode;
    QSGVertexColorMaterial *m_underMaterial;
    GlyphMaterial *m_glyphMaterial;
    QSGTextureMaterial *m_colorGlyphMaterial;
    QSGVertexColorMaterial *m_overMaterial;

    std::vector<QSGGeometry::ColoredPoint2D> m_under;
    std::vector<GlyphVertex> m_glyphs;
    std::vector<QSGGeometry::TexturedPoint2D> m_colorGlyphs;
    std::vector<QSGGeometry::ColoredPoint2D> m_over;

    qreal m_dpr = 1;
    QPointF m_origin;

    // The atlas, packed in shelves; a full atlas starts over.
    QFont m_atlasFont;
    qreal m_atlasDpr = 0;
    int m_atlasAscent = 0;
    int m_atlasLineSpacing = 0;
    int m_atlasFontWidth = 1;
    int m_atlasFontHeight = 1;
    QImage m_atlas;
    bool m_atlasDirty = false;
    bool m_atlasFull = false;
    int m_shelfX = 0;
    int m_shelfY = 0;
    int m_shelfHeight = 0;
    std::vector<Glyph> m_glyphTable;
    QHash<quint64, int> m_singleGlyphs;
    QHash<QString, int> m_clusterGlyphs;
    QSGTexture *m_texture = nullptr;
};

}

#endif // TERMINALGPUNODE_H
